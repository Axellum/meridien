// Noyau pur : décodeur INCRÉMENTAL de flux SSE (Server-Sent Events), tel que le renvoient les services compatibles
// OpenAI pour `chat/completions` avec `"stream": true`. C++17 standard, aucun include ESP-IDF, aucune allocation
// (les tampons sont dans l'objet), évaluable à la compilation.
//
// Règles suivies (WHATWG HTML, « Server-sent events », interprétation du flux) :
//  - fins de ligne CRLF, LF ou CR seul ; un CR coupé de son LF par la frontière de deux morceaux est géré ;
//  - une ligne vide expédie l'événement en cours ; une ligne qui commence par « : » est un commentaire (ignoré,
//    c'est le « keep-alive » de certains serveurs) ;
//  - « data: » peut revenir sur plusieurs lignes : les valeurs sont jointes par « \n » ; un seul espace après les
//    deux-points est retiré ; « data » sans deux-points = valeur vide ; champs inconnus et « id »/« retry » ignorés ;
//  - un événement sans aucune ligne « data » n'est pas expédié ;
//  - un événement dont la ligne n'est pas terminée quand le flux s'arrête est perdu (`evenement_incomplet()`).
// Convention OpenAI : `data: [DONE]` ferme le flux (`EvenementSse::fin`) ; après lui, plus rien n'est lu.
//
// Les morceaux peuvent être coupés n'importe où, y compris au milieu d'un caractère UTF-8 ou entre un CR et son
// LF : le décodeur travaille octet par octet et ne cherche que des octets ASCII (« \r », « \n », « : », « ' ' »),
// qui n'apparaissent jamais à l'intérieur d'une séquence UTF-8 multi-octets. Il ne REFORMATE pas le texte : les
// données sont rendues telles quelles (la validation UTF-8 se fait en aval, voir extrait_openai.hpp).
//
// Limites : pas de gestion de la marque d'ordre des octets (BOM) en tête de flux (OpenAI n'en envoie pas, non
// vérifié sur les autres serveurs) ; un événement plus gros que CAPACITE_DONNEES est signalé `trop_long` avec des
// données vides plutôt que tronqué (un JSON coupé serait pire qu'absent).
#pragma once

#include <cstddef>
#include <string_view>

namespace ecran::assistant {

struct EvenementSse {
    std::string_view type;    // champ « event: » ; vide = « message » (cas des services OpenAI)
    std::string_view donnees; // valeurs de « data: », jointes par '\n' ; vide si `trop_long`
    bool fin;                 // `donnees` vaut exactement « [DONE] »
    bool trop_long;           // l'événement dépassait la capacité du décodeur
};

// Les vues de `EvenementSse` ne valent que pendant l'appel de `sur_evenement`.
template <std::size_t CAPACITE_DONNEES, std::size_t CAPACITE_TYPE = 32> class DecodeurSse {
    static_assert(CAPACITE_DONNEES >= 16, "capacité trop petite pour un événement utile");

  public:
    // Alimente le décodeur avec un morceau d'octets (taille quelconque, même 1 octet, même vide). Appelle
    // `sur_evenement(const EvenementSse&)` pour chaque événement complet. Rend le nombre d'événements expédiés.
    template <class F> constexpr std::size_t alimenter(std::string_view morceau, F &&sur_evenement) {
        std::size_t expedies = 0;
        for (char c : morceau) {
            if (termine_)
                break;
            if (saute_lf_) {
                saute_lf_ = false;
                if (c == '\n')
                    continue; // le LF d'un CRLF dont le CR était dans le morceau précédent
            }
            if (c == '\r' || c == '\n') {
                saute_lf_ = (c == '\r');
                if (fin_de_ligne()) {
                    expedier(sur_evenement);
                    ++expedies;
                }
                continue;
            }
            octet(c);
        }
        return expedies;
    }

    // Vrai après `data: [DONE]` : le flux est fini, tout ce qui suit est ignoré.
    constexpr bool termine() const noexcept { return termine_; }

    // Vrai si des octets d'un événement non terminé attendent (le flux s'est arrêté trop tôt si on s'arrête là).
    constexpr bool evenement_incomplet() const noexcept {
        return !(etat_ == Etat::DebutLigne && !a_donnees_ && type_n_ == 0);
    }

    constexpr void remettre_a_zero() noexcept {
        etat_ = Etat::DebutLigne;
        champ_ = Champ::Aucun;
        nom_n_ = 0;
        nom_trop_long_ = false;
        saute_lf_ = false;
        termine_ = false;
        vider_evenement();
    }

  private:
    enum class Etat { DebutLigne, Nom, AvantValeur, Valeur, Commentaire };
    enum class Champ { Aucun, Data, Event };

    constexpr void vider_evenement() noexcept {
        data_n_ = 0;
        type_n_ = 0;
        a_donnees_ = false;
        trop_long_ = false;
    }

    constexpr Champ champ_du_nom() const noexcept {
        if (nom_trop_long_)
            return Champ::Aucun;
        const std::string_view nom(nom_, nom_n_);
        if (nom == "data")
            return Champ::Data;
        if (nom == "event")
            return Champ::Event;
        return Champ::Aucun;
    }

    constexpr void debut_ligne_data() noexcept {
        if (a_donnees_)
            ecrire_donnee('\n');
        a_donnees_ = true;
    }

    constexpr void ecrire_donnee(char c) noexcept {
        if (trop_long_)
            return;
        if (data_n_ >= CAPACITE_DONNEES) {
            trop_long_ = true;
            return;
        }
        data_[data_n_++] = c;
    }

    // Octet ordinaire (ni CR ni LF).
    constexpr void octet(char c) noexcept {
        switch (etat_) {
        case Etat::DebutLigne:
            if (c == ':') {
                etat_ = Etat::Commentaire;
            } else {
                nom_n_ = 0;
                nom_trop_long_ = false;
                etat_ = Etat::Nom;
                ajouter_au_nom(c);
            }
            break;
        case Etat::Nom:
            if (c == ':') {
                champ_ = champ_du_nom();
                if (champ_ == Champ::Data)
                    debut_ligne_data();
                if (champ_ == Champ::Event)
                    type_n_ = 0;
                etat_ = Etat::AvantValeur;
            } else {
                ajouter_au_nom(c);
            }
            break;
        case Etat::AvantValeur:
            etat_ = Etat::Valeur;
            if (c != ' ')
                valeur(c); // un seul espace est retiré après les deux-points
            break;
        case Etat::Valeur:
            valeur(c);
            break;
        case Etat::Commentaire:
            break;
        }
    }

    constexpr void ajouter_au_nom(char c) noexcept {
        if (nom_n_ < sizeof(nom_)) {
            nom_[nom_n_++] = c;
        } else {
            nom_trop_long_ = true;
        }
    }

    constexpr void valeur(char c) noexcept {
        if (champ_ == Champ::Data) {
            ecrire_donnee(c);
        } else if (champ_ == Champ::Event && type_n_ < CAPACITE_TYPE) {
            type_[type_n_++] = c;
        }
    }

    // Fin de ligne ; rend vrai si la ligne vide demande d'expédier l'événement.
    constexpr bool fin_de_ligne() noexcept {
        switch (etat_) {
        case Etat::DebutLigne: // ligne vide : expédier ; sans aucune ligne « data », rien à expédier et le type est
                               // oublié
            if (!a_donnees_)
                type_n_ = 0;
            return a_donnees_;
        case Etat::Nom: // champ sans deux-points : nom seul, valeur vide
            champ_ = champ_du_nom();
            if (champ_ == Champ::Data)
                debut_ligne_data();
            if (champ_ == Champ::Event)
                type_n_ = 0;
            break;
        case Etat::AvantValeur:
        case Etat::Valeur:
        case Etat::Commentaire:
            break;
        }
        etat_ = Etat::DebutLigne;
        return false;
    }

    template <class F> constexpr void expedier(F &sur_evenement) {
        const std::string_view donnees = trop_long_ ? std::string_view() : std::string_view(data_, data_n_);
        const EvenementSse ev{std::string_view(type_, type_n_), donnees, !trop_long_ && donnees == "[DONE]",
                              trop_long_};
        if (ev.fin)
            termine_ = true;
        sur_evenement(ev);
        vider_evenement();
    }

    Etat etat_ = Etat::DebutLigne;
    Champ champ_ = Champ::Aucun;
    char nom_[8] = {};
    std::size_t nom_n_ = 0;
    bool nom_trop_long_ = false;
    char data_[CAPACITE_DONNEES] = {};
    std::size_t data_n_ = 0;
    char type_[CAPACITE_TYPE] = {};
    std::size_t type_n_ = 0;
    bool a_donnees_ = false;
    bool trop_long_ = false;
    bool saute_lf_ = false;
    bool termine_ = false;
};

} // namespace ecran::assistant
