// Noyau pur : extraction ciblée de trois informations dans les réponses JSON d'un service compatible OpenAI :
//   - `choices[0].delta.content`  (un morceau de réponse du LLM, dans un événement SSE) ;
//   - `text`                      (la transcription rendue par `audio/transcriptions`, au format json) ;
//   - `error.message`             (le texte d'erreur du service ; aussi `error` seul si c'est une chaîne, et un
//                                  tableau en racine dont le premier élément est l'objet, façon Google).
//
// NOTE D'UNIFICATION : ce lecteur JSON est VOLONTAIREMENT MINIMAL et propre à ce fichier (espace de noms `detail`).
// Une autre piste écrit un `json_mini.hpp` commun : une fois celui-ci en place, remplacer `detail::Lecteur` par lui
// et garder les trois fonctions publiques ci-dessous (leurs tests, test/host/test_extrait_openai.cpp, restent
// valables tels quels). Ne pas attendre : ce fichier est autonome.
//
// Comportement : navigation par clés exactes dans les seuls objets traversés (donc « reasoning_content » ne
// passe JAMAIS pour « content » : le raisonnement d'un modèle local n'est pas lu à voix haute) ; les autres
// valeurs sont sautées sans être interprétées ; premier élément du tableau `choices` seulement ; `content`
// absent ou `null` (premier morceau « role », dernier morceau « finish_reason ») = `Absent`, pas une erreur.
// Les chaînes JSON sont décodées (\n \" \\ \/ \b \f \r \t, \uXXXX et paires de surrogates -> UTF-8 ; surrogate
// isolé ou octet UTF-8 invalide -> U+FFFD ; \u0000 ignoré) dans le tableau de l'appelant. Si le texte ne tient
// pas : `TropLong`, jamais un texte tronqué. Tolérant sur la forme (un JSON mal fermé après la valeur cherchée
// n'est pas vu), strict sur la valeur cherchée.
//
// SÉCURITÉ (docs/api-reglages.md §1) : le message d'erreur d'un service peut citer la clé ; il sert à CLASSER
// l'erreur (`mentionne_modele`), jamais à l'afficher ni à le journaliser.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/assistant_texte.hpp"

namespace ecran::assistant {

enum class Lecture : std::uint8_t {
    Texte,     // trouvé : `Extrait::texte` est rempli
    Absent,    // chemin absent, ou valeur `null` : normal (premier et dernier morceaux d'un flux)
    Invalide,  // JSON illisible à cet endroit, ou valeur de mauvais type
    TropLong   // la chaîne décodée ne tient pas dans le tableau fourni
};

struct Extrait {
    Lecture lecture;
    std::string_view texte;  // vue dans le tableau `sortie` de l'appelant ; vide sauf si `lecture == Texte`
};

namespace detail {

struct Etape {
    std::string_view cle;  // vide = « premier élément du tableau »
};

class Lecteur {
public:
    constexpr explicit Lecteur(std::string_view s) noexcept : s_(s) {}

    constexpr void sauter_blancs() noexcept {
        while (i_ < s_.size() && (s_[i_] == ' ' || s_[i_] == '\t' || s_[i_] == '\n' || s_[i_] == '\r')) ++i_;
    }
    constexpr bool fin() const noexcept { return i_ >= s_.size(); }
    constexpr char courant() const noexcept { return i_ < s_.size() ? s_[i_] : '\0'; }

    // Consomme `c` (après les blancs) si c'est le caractère courant.
    constexpr bool consommer(char c) noexcept {
        sauter_blancs();
        if (i_ < s_.size() && s_[i_] == c) {
            ++i_;
            return true;
        }
        return false;
    }

    // Consomme le mot `mot` (null, true…) s'il est là, suivi d'une fin de valeur.
    constexpr bool consommer_mot(std::string_view mot) noexcept {
        sauter_blancs();
        if (s_.size() - i_ < mot.size() || s_.substr(i_, mot.size()) != mot) return false;
        const std::size_t apres = i_ + mot.size();
        if (apres < s_.size()) {
            const char c = s_[apres];
            if (c != ',' && c != '}' && c != ']' && c != ' ' && c != '\t' && c != '\n' && c != '\r') return false;
        }
        i_ = apres;
        return true;
    }

    constexpr bool sauter_chaine() noexcept {  // positionné sur le guillemet ouvrant
        ++i_;
        while (i_ < s_.size()) {
            const char c = s_[i_];
            if (c == '\\') {
                i_ += 2;
            } else if (c == '"') {
                ++i_;
                return true;
            } else {
                ++i_;
            }
        }
        return false;
    }

    // Saute une valeur JSON entière (chaîne, objet, tableau, nombre, mot). Faux si elle est tronquée.
    constexpr bool sauter_valeur() noexcept {
        sauter_blancs();
        if (fin()) return false;
        char c = s_[i_];
        if (c == '"') return sauter_chaine();
        if (c == '{' || c == '[') {
            std::size_t profondeur = 0;
            while (i_ < s_.size()) {
                c = s_[i_];
                if (c == '"') {
                    if (!sauter_chaine()) return false;
                    continue;
                }
                if (c == '{' || c == '[') {
                    ++profondeur;
                } else if (c == '}' || c == ']') {
                    if (profondeur > 0) --profondeur;
                    if (profondeur == 0) {
                        ++i_;
                        return true;
                    }
                }
                ++i_;
            }
            return false;
        }
        const std::size_t debut = i_;
        while (i_ < s_.size() && s_[i_] != ',' && s_[i_] != '}' && s_[i_] != ']' && s_[i_] != ' ' && s_[i_] != '\t' &&
               s_[i_] != '\n' && s_[i_] != '\r') {
            ++i_;
        }
        return i_ > debut;
    }

    // Juste après un « { » : avance jusqu'à la valeur de la clé `cle` (position sur la valeur). Absent si l'objet
    // se ferme sans elle.
    constexpr Lecture trouver(std::string_view cle) noexcept {
        while (true) {
            sauter_blancs();
            if (fin()) return Lecture::Invalide;
            if (s_[i_] == '}') {
                ++i_;
                return Lecture::Absent;
            }
            if (s_[i_] != '"') return Lecture::Invalide;
            const std::size_t debut = i_ + 1;
            if (!sauter_chaine()) return Lecture::Invalide;
            const std::string_view nom = s_.substr(debut, i_ - 1 - debut);
            if (!consommer(':')) return Lecture::Invalide;
            sauter_blancs();
            if (nom == cle) return Lecture::Texte;  // « Texte » = trouvé (position sur la valeur)
            if (!sauter_valeur()) return Lecture::Invalide;
            sauter_blancs();
            if (consommer(',')) continue;
            if (consommer('}')) return Lecture::Absent;
            return Lecture::Invalide;
        }
    }

    // Décode la chaîne JSON qui commence au guillemet courant, dans `sortie`.
    constexpr Lecture decoder_chaine(Tampon& sortie) noexcept {
        if (i_ >= s_.size() || s_[i_] != '"') return Lecture::Invalide;
        ++i_;
        while (i_ < s_.size()) {
            const char c = s_[i_];
            if (c == '"') {
                ++i_;
                return sortie.ok() ? Lecture::Texte : Lecture::TropLong;
            }
            if (c == '\\') {
                if (i_ + 1 >= s_.size()) return Lecture::Invalide;
                const char e = s_[i_ + 1];
                i_ += 2;
                switch (e) {
                    case '"': sortie.ecrire('"'); break;
                    case '\\': sortie.ecrire('\\'); break;
                    case '/': sortie.ecrire('/'); break;
                    case 'b': sortie.ecrire('\b'); break;
                    case 'f': sortie.ecrire('\f'); break;
                    case 'n': sortie.ecrire('\n'); break;
                    case 'r': sortie.ecrire('\r'); break;
                    case 't': sortie.ecrire('\t'); break;
                    case 'u': {
                        std::uint32_t cp = 0;
                        if (!hex4(cp)) return Lecture::Invalide;
                        if (cp >= 0xD800U && cp <= 0xDBFFU) {  // première moitié d'une paire de surrogates
                            std::uint32_t bas = 0;
                            const std::size_t retour = i_;
                            if (i_ + 1 < s_.size() && s_[i_] == '\\' && s_[i_ + 1] == 'u') {
                                i_ += 2;
                                if (hex4(bas) && bas >= 0xDC00U && bas <= 0xDFFFU) {
                                    cp = 0x10000U + ((cp - 0xD800U) << 10) + (bas - 0xDC00U);
                                } else {
                                    i_ = retour;  // pas une paire : le second \u sera relu seul
                                    cp = 0xFFFDU;
                                }
                            } else {
                                cp = 0xFFFDU;
                            }
                        } else if (cp >= 0xDC00U && cp <= 0xDFFFU) {
                            cp = 0xFFFDU;  // seconde moitié sans la première
                        }
                        if (cp != 0) {  // \u0000 : ignoré
                            char tmp[4] = {};
                            const std::size_t n = utf8::encoder(cp, tmp);
                            sortie.ecrire(std::string_view(tmp, n));
                        }
                        break;
                    }
                    default: return Lecture::Invalide;
                }
                continue;
            }
            if (static_cast<unsigned char>(c) >= 0x80U) {
                const std::size_t n = utf8::sequence_valide(s_, i_);
                if (n == 0) {
                    sortie.ecrire("\xEF\xBF\xBD");
                    ++i_;
                } else {
                    sortie.ecrire(s_.substr(i_, n));
                    i_ += n;
                }
                continue;
            }
            sortie.ecrire(c);
            ++i_;
        }
        return Lecture::Invalide;  // chaîne jamais fermée
    }

private:
    constexpr bool hex4(std::uint32_t& valeur) noexcept {
        if (i_ + 4 > s_.size()) return false;
        std::uint32_t v = 0;
        for (std::size_t k = 0; k < 4; ++k) {
            const char c = s_[i_ + k];
            std::uint32_t d = 0;
            if (c >= '0' && c <= '9') {
                d = static_cast<std::uint32_t>(c - '0');
            } else if (c >= 'a' && c <= 'f') {
                d = static_cast<std::uint32_t>(c - 'a' + 10);
            } else if (c >= 'A' && c <= 'F') {
                d = static_cast<std::uint32_t>(c - 'A' + 10);
            } else {
                return false;
            }
            v = (v << 4) | d;
        }
        i_ += 4;
        valeur = v;
        return true;
    }

    std::string_view s_;
    std::size_t i_ = 0;
};

// Descend le long de `etapes` depuis la racine. `Texte` = trouvé : le lecteur est sur la valeur finale (non
// consommée). Une étape à clé attend un objet (ou `null`, d'où `Absent`) et se place sur la valeur de la clé ; une
// étape vide attend un tableau et se place sur son premier élément. `tolere_tableau_racine` : une racine
// « [ { … } ] » est lue comme son premier élément.
constexpr Lecture descendre(Lecteur& l, const Etape* etapes, std::size_t n, bool tolere_tableau_racine) noexcept {
    if (tolere_tableau_racine && l.consommer('[')) {
        l.sauter_blancs();
        if (l.courant() == ']') return Lecture::Absent;
    }
    for (std::size_t k = 0; k < n; ++k) {
        if (etapes[k].cle.empty()) {
            if (!l.consommer('[')) return l.consommer_mot("null") ? Lecture::Absent : Lecture::Invalide;
            l.sauter_blancs();
            if (l.courant() == ']') return Lecture::Absent;
        } else {
            if (!l.consommer('{')) return l.consommer_mot("null") ? Lecture::Absent : Lecture::Invalide;
            const Lecture r = l.trouver(etapes[k].cle);
            if (r != Lecture::Texte) return r;
        }
    }
    return Lecture::Texte;
}

// La valeur finale doit être une chaîne ou `null` (-> Absent).
constexpr Extrait lire_chaine_finale(Lecteur& l, char* sortie, std::size_t capacite) noexcept {
    Tampon t(sortie, capacite);
    l.sauter_blancs();
    if (l.consommer_mot("null")) return {Lecture::Absent, std::string_view()};
    const Lecture r = l.decoder_chaine(t);
    if (r != Lecture::Texte) return {r, std::string_view()};
    return {Lecture::Texte, t.vue()};
}

constexpr bool est_blanc(char c) noexcept { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

constexpr std::string_view rogner(std::string_view s) noexcept {
    while (!s.empty() && est_blanc(s.front())) s.remove_prefix(1);
    while (!s.empty() && est_blanc(s.back())) s.remove_suffix(1);
    return s;
}

constexpr bool contient_sans_casse(std::string_view texte, std::string_view motif_minuscule) noexcept {
    if (motif_minuscule.empty() || texte.size() < motif_minuscule.size()) return false;
    for (std::size_t i = 0; i + motif_minuscule.size() <= texte.size(); ++i) {
        bool ok = true;
        for (std::size_t k = 0; k < motif_minuscule.size() && ok; ++k) {
            char c = texte[i + k];
            if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');  // casse ASCII seulement
            ok = (c == motif_minuscule[k]);
        }
        if (ok) return true;
    }
    return false;
}

}  // namespace detail

// `choices[0].delta.content` d'un événement SSE de `chat/completions`. `sortie`/`capacite` : tableau de l'appelant
// qui reçoit le texte décodé (`Extrait::texte` est une vue dedans). `Absent` pour un morceau sans texte.
constexpr Extrait contenu_du_delta(std::string_view json, char* sortie, std::size_t capacite) noexcept {
    constexpr detail::Etape CHEMIN[] = {{"choices"}, {""}, {"delta"}, {"content"}};
    detail::Lecteur l(json);
    const Lecture r = detail::descendre(l, CHEMIN, 4, false);
    if (r != Lecture::Texte) return {r, std::string_view()};
    return detail::lire_chaine_finale(l, sortie, capacite);
}

// Champ `text` d'une réponse de `audio/transcriptions` (format json), espaces de bord retirés (whisper.cpp en met).
// Un texte vide après rognage reste `Texte` avec une vue vide : l'appelant envoie alors `RienReconnu`.
constexpr Extrait texte_transcription(std::string_view json, char* sortie, std::size_t capacite) noexcept {
    constexpr detail::Etape CHEMIN[] = {{"text"}};
    detail::Lecteur l(json);
    const Lecture r = detail::descendre(l, CHEMIN, 1, false);
    if (r != Lecture::Texte) return {r, std::string_view()};
    const Extrait e = detail::lire_chaine_finale(l, sortie, capacite);
    if (e.lecture != Lecture::Texte) return e;
    return {Lecture::Texte, detail::rogner(e.texte)};
}

// `error.message` (ou `error` si c'est une chaîne) d'un corps d'erreur. NE JAMAIS afficher ni journaliser ce texte.
constexpr Extrait message_d_erreur(std::string_view json, char* sortie, std::size_t capacite) noexcept {
    constexpr detail::Etape ERREUR[] = {{"error"}};
    constexpr detail::Etape MESSAGE[] = {{"message"}};
    detail::Lecteur l(json);
    Lecture r = detail::descendre(l, ERREUR, 1, true);
    if (r != Lecture::Texte) return {r, std::string_view()};
    l.sauter_blancs();
    if (l.courant() != '"') {
        r = detail::descendre(l, MESSAGE, 1, false);
        if (r != Lecture::Texte) return {r, std::string_view()};
    }
    return detail::lire_chaine_finale(l, sortie, capacite);
}

// Le message d'erreur parle-t-il du modèle ? (« The model `x` does not exist », « model 'x' not found », « Modèle
// introuvable »…) Sert à choisir la phrase « Ce modèle n'existe pas sur ce service » (api §8). Casse ASCII ignorée.
constexpr bool mentionne_modele(std::string_view message) noexcept {
    return detail::contient_sans_casse(message, "model") || detail::contient_sans_casse(message, "modèle");
}

}  // namespace ecran::assistant
