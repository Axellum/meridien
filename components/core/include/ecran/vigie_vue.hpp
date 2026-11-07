// Noyau pur : quelles alertes montrer MAINTENANT, dans quel ordre, avec quels mots. C++17 standard, aucun include ESP-IDF ni LVGL, aucune
// allocation, évaluable à la compilation. S'appuie sur vigie_alertes.hpp (la lecture de la réponse de Vigie).
//
// « En vigueur » (ADR-0009) : une alerte est montrée seulement si, à `maintenant_s`,
//   - la réponse qui la porte a moins de AGE_MAX_S (6 h) : au-delà on ne sait plus si l'avertissement tient encore, l'écran se tait ;
//   - elle n'est pas annulée (`Etat::Annule`) et son niveau n'est pas « vert » ;
//   - son début est passé : `debut` (onset), à défaut `effectif` ; un début inconnu compte comme « déjà commencée » (l'émetteur la
//     publie dans son flux) ; une alerte qui commence DANS LE FUTUR n'est pas montrée (l'écran ne parle que de ce qui est là) ;
//   - sa fin n'est pas passée : `fin` (ends), à défaut `expire` (expires), la fin étant EXCLUE sauf à « hh:59:59 » (voir
//     `fin_exclue_s`) ; une fin inconnue la laisse en vigueur, tant que la réponse reste fraîche (condition du premier point : une
//     alerte sans fin ne reste jamais plus de 6 h sans nouvelle de Vigie).
// Le niveau doit atteindre le niveau minimal du réglage ; un niveau INCONNU (absent ou hors de la table de Vigie) n'est montré que
// pour le réglage « Jaune » (le plus bas), en couleur neutre : on ne le classe pas à la place de l'émetteur.
//
// Les mots : l'écran montre en priorité ceux de l'ÉMETTEUR (`titre_de` : titre du texte dans la langue de l'écran, sinon en anglais,
// sinon le premier ; à défaut `event`, puis le phénomène d'origine) ; en dernier recours les mots de Vigie, dérivés (vigie_textes.hpp).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/vigie_alertes.hpp"

namespace ecran::vigie {

// Réglage « niveau minimal » : les valeurs sont celles enregistrées en NVS (ne jamais les renuméroter).
enum class NiveauMin : std::uint8_t { Jaune = 0, Orange = 1, Rouge = 2 };
inline constexpr std::size_t NOMBRE_NIVEAUX_MIN = 3;

constexpr NiveauMin niveau_min_depuis_indice(int i, NiveauMin defaut = NiveauMin::Jaune) noexcept {
    return (i >= 0 && i < static_cast<int>(NOMBRE_NIVEAUX_MIN)) ? static_cast<NiveauMin>(i) : defaut;
}

inline constexpr std::int64_t AGE_MAX_S = 6 * 3600;

// Le niveau atteint-il le minimum demandé ? « Vert » jamais. Inconnu : seulement pour « Jaune ».
constexpr bool niveau_suffisant(Niveau n, NiveauMin minimum) noexcept {
    switch (minimum) {
    case NiveauMin::Jaune:
        return n == Niveau::Inconnu || n >= Niveau::Jaune;
    case NiveauMin::Orange:
        return n >= Niveau::Orange;
    case NiveauMin::Rouge:
        return n >= Niveau::Rouge;
    }
    return false;
}

// L'instant (secondes UTC) à partir duquel une alerte de fin `fin` n'est plus en vigueur : la fin elle-même, sauf quand l'émetteur
// l'écrit à « hh:59:59 », la dernière seconde d'une heure pleine (l'AEMET), alors que le suivant commence à « hh:00:00 » (le DWD) :
// cette seconde-là est encore dans l'alerte. Sans cela deux avertissements qui se suivent ainsi laissent l'écran muet UNE seconde (la
// pastille disparaît puis revient). Toute autre fin reste exclue : on ne prolonge pas une alerte au-delà de ce que l'émetteur a écrit.
constexpr std::int64_t fin_exclue_s(std::int64_t fin) noexcept { return (fin + 1) % 3600 == 0 ? fin + 1 : fin; }

// L'alerte est-elle en vigueur à `maintenant_s` (secondes UTC), la réponse qui la porte ayant `age_s` secondes ?
constexpr bool en_vigueur(const Alerte &a, std::int64_t maintenant_s, std::int64_t age_s) noexcept {
    if (age_s < 0 || age_s > AGE_MAX_S)
        return false;
    if (a.etat == Etat::Annule || a.niveau == Niveau::Vert)
        return false;
    const std::optional<std::int64_t> debut = a.debut ? a.debut : a.effectif;
    if (debut && maintenant_s < *debut)
        return false;
    const std::optional<std::int64_t> fin = a.fin ? a.fin : a.expire;
    if (fin && maintenant_s >= fin_exclue_s(*fin))
        return false;
    return true;
}

struct Vue {
    std::uint8_t nombre = 0; // alertes à montrer
    std::uint8_t indices[ALERTES_MAX] =
        {};                        // leurs rangs dans Alertes::alertes : la plus grave d'abord, puis la plus ancienne
    Niveau pire = Niveau::Inconnu; // niveau de la première (Inconnu si `nombre` vaut 0)
    constexpr bool vide() const noexcept { return nombre == 0; }
};

// Les alertes de `a` à montrer à `maintenant_s` (secondes UTC ; 0 ou moins : l'heure n'est pas connue, rien n'est montré), la réponse
// ayant été reçue à `recu_s` (même horloge). `a.statut` autre que Ok : rien.
constexpr Vue composer(const Alertes &a, std::int64_t maintenant_s, std::int64_t recu_s, NiveauMin minimum) noexcept {
    Vue v;
    if (a.statut != Statut::Ok || maintenant_s <= 0 || recu_s <= 0)
        return v;
    const std::int64_t age = maintenant_s - recu_s;
    for (std::size_t i = 0; i < a.nombre && i < ALERTES_MAX; ++i) {
        const Alerte &x = a.alertes[i];
        if (!niveau_suffisant(x.niveau, minimum) || !en_vigueur(x, maintenant_s, age))
            continue;
        // Insertion stable : plus grave d'abord, puis début le plus ancien (inconnu = le plus ancien), puis l'ordre de la réponse.
        std::size_t place = v.nombre;
        while (place > 0) {
            const Alerte &avant = a.alertes[v.indices[place - 1]];
            const bool passe_devant =
                x.niveau > avant.niveau || (x.niveau == avant.niveau && x.debut.value_or(0) < avant.debut.value_or(0));
            if (!passe_devant)
                break;
            v.indices[place] = v.indices[place - 1];
            --place;
        }
        v.indices[place] = static_cast<std::uint8_t>(i);
        ++v.nombre;
    }
    if (v.nombre > 0)
        v.pire = a.alertes[v.indices[0]].niveau;
    return v;
}

// Le texte de l'émetteur à montrer pour `langue` (« fr ») : celui de la langue de l'écran, sinon l'anglais, sinon le premier ;
// nullptr si l'alerte n'a aucun texte dessinable.
constexpr const Texte *choisir_texte(const Alerte &a, std::string_view langue) noexcept {
    if (a.nb_textes == 0)
        return nullptr;
    for (std::size_t i = 0; i < a.nb_textes && i < TEXTES_MAX; ++i) {
        if (langue_correspond(texte_de(a.textes[i].langue), langue))
            return &a.textes[i];
    }
    for (std::size_t i = 0; i < a.nb_textes && i < TEXTES_MAX; ++i) {
        if (langue_correspond(texte_de(a.textes[i].langue), "en"))
            return &a.textes[i];
    }
    return &a.textes[0];
}

enum class OrigineTitre : std::uint8_t {
    Aucune = 0,        // l'émetteur n'a rien dit de dessinable : l'écran compose avec les mots de Vigie
    TitreEmetteur,     // le titre du texte de l'émetteur
    Evenement,         // `event` de l'émetteur
    PhenomeneOriginal, // le phénomène comme l'émetteur l'écrit
};

struct Titre {
    std::string_view texte;
    OrigineTitre origine = OrigineTitre::Aucune;
};

// Le titre d'une alerte, en mots de l'émetteur quand il y en a.
constexpr Titre titre_de(const Alerte &a, std::string_view langue) noexcept {
    const Texte *t = choisir_texte(a, langue);
    if (t != nullptr && t->titre[0] != '\0')
        return {texte_de(t->titre), OrigineTitre::TitreEmetteur};
    if (a.evenement[0] != '\0')
        return {texte_de(a.evenement), OrigineTitre::Evenement};
    if (a.phenomene_original[0] != '\0')
        return {texte_de(a.phenomene_original), OrigineTitre::PhenomeneOriginal};
    return {};
}

// Empreinte (FNV-1a 32 bits) de ce que la bande et la page montrent : change quand une alerte apparaît, finit, change de niveau, de
// texte ou de langue. Sert à ne redessiner que si quelque chose a changé.
constexpr std::uint32_t empreinte(const Alertes &a, const Vue &v, std::string_view langue) noexcept {
    std::uint32_t h = 2166136261u;
    const auto octet = [&h](std::uint32_t o) { h = (h ^ (o & 0xFFu)) * 16777619u; };
    const auto mot = [&octet](std::int64_t x) {
        for (int i = 0; i < 8; ++i)
            octet(static_cast<std::uint32_t>(static_cast<std::uint64_t>(x) >> (8 * i)));
    };
    const auto chaine = [&octet](std::string_view s) {
        for (const char c : s)
            octet(static_cast<unsigned char>(c));
        octet(0);
    };
    octet(v.nombre);
    for (std::size_t i = 0; i < v.nombre; ++i) {
        const Alerte &x = a.alertes[v.indices[i]];
        octet(static_cast<std::uint32_t>(x.niveau));
        octet(static_cast<std::uint32_t>(x.phenomene));
        mot(x.debut.value_or(-1));
        mot(x.fin.value_or(-1));
        mot(x.expire.value_or(-1));
        chaine(titre_de(x, langue).texte);
        const Texte *t = choisir_texte(x, langue);
        if (t != nullptr) {
            chaine(texte_de(t->description));
            chaine(texte_de(t->consigne));
        }
        chaine(texte_de(x.zone));
        chaine(texte_de(x.emetteur));
        chaine(texte_de(x.attribution));
    }
    chaine(texte_de(a.attribution));
    chaine(texte_de(a.avertissement));
    return h;
}

} // namespace ecran::vigie
