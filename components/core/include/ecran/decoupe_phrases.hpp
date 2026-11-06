// Noyau pur : découpe en PHRASES COMPLÈTES la réponse du LLM qui arrive par petits morceaux, pour les envoyer au
// TTS dès qu'elles sont finies (le TTS commence à parler avant la fin de la réponse). C++17 standard, aucun
// include ESP-IDF, aucune allocation (un tampon de LIMITE octets dans l'objet), évaluable à la compilation.
//
// Une phrase se termine à « . », « ! », « ? », « … » (ou une suite comme « ?! », « ... »), éventuellement suivis de
// guillemets/parenthèses fermants, À CONDITION d'être suivie d'un blanc : « 3.5 » et « exemple.fr » ne coupent pas.
// Un retour à la ligne coupe toujours. Tant que le blanc de confirmation n'est pas arrivé (le morceau s'arrête
// juste après le point), on ATTEND : la coupe ne dépend donc pas de la façon dont le texte est découpé en morceaux
// (propriété testée pour toutes les coupures possibles).
//
// Pas de coupe après un point qui n'en est pas un :
//   - numéro de liste : 1 ou 2 chiffres seuls (« 1. Pommes », « 12. Fin ») ;
//   - initiale : une seule lettre majuscule (« M. Dupont », « J. Martin ») ;
//   - abréviation courante (liste `ABREVIATIONS`, casse ASCII ignorée) : M., Mme, Mlle, Dr, Pr, St, Cie, etc.,
//     cf., p. ex., vs., av., apr., tél., fig., art.… ; ou forme à points internes de 1 à 2 lettres (« p.ex. »,
//     « U.S.A. », « i.e. »). Une abréviation en fin de phrase ne coupe donc pas non plus (« …des pommes, etc. Puis »)
//     : le texte est alors fusionné avec la phrase suivante, ce qui est sans conséquence pour la voix, alors qu'une
//     coupe de trop enverrait « Mme. » seule au TTS.
// Les phrases sans aucune lettre ni chiffre (« … », « - », « *** ») ne sont pas émises. Limite connue : une phrase qui
// finit par un nombre de 1 ou 2 chiffres (« …page 3. Puis… ») n'est pas coupée (confondue avec un numéro de liste).
//
// Borne de longueur : jamais plus de LIMITE octets en attente. Quand le tampon est plein sans coupe naturelle, on
// coupe après la dernière virgule/point-virgule/deux-points suivi d'une espace de la seconde moitié, sinon à la
// dernière espace de la seconde moitié, sinon au dernier caractère entier (jamais au milieu d'un caractère UTF-8).
//
// Hors périmètre (non fait) : retirer la mise en forme Markdown (« ** », « # », puces) avant la voix ; ce n'est pas
// de la découpe. À traiter en amont ou en aval.
//
// Les textes sont en UTF-8 : sous MSVC, compiler avec /utf-8.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/assistant_texte.hpp"

namespace ecran::assistant {

namespace detail {

// Abréviations françaises qui précèdent un point sans finir la phrase (en minuscules ASCII ; « tél » en UTF-8).
inline constexpr std::string_view ABREVIATIONS[] = {
    "m",    "mm",   "mme",  "mmes", "mlle", "mlles", "me",   "dr",   "drs",  "pr",   "prs",  "mgr",  "st",
    "ste",  "sts",  "stes", "cie",  "cf",   "etc",   "ex",   "env",  "av",   "apr",  "ap",   "vs",   "fig",  "vol",
    "chap", "art",  "tél",  "tel",  "bd",   "boul",  "pp",   "p",    "n",    "no",   "nos",  "approx", "qqch", "qqn",
    "ibid", "op",   "cit",  "trad", "suiv", "sq",    "sqq",  "resp", "jr",   "sr",   "inc",  "ltd",  "co",   "mr",
    "mrs",  "ms",   "j.-c", "c.-à-d"};

constexpr bool est_blanc_phrase(char c) noexcept { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

constexpr char minuscule_ascii(char c) noexcept { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }

constexpr bool meme_mot_sans_casse(std::string_view mot, std::string_view modele_minuscule) noexcept {
    if (mot.size() != modele_minuscule.size()) return false;
    for (std::size_t i = 0; i < mot.size(); ++i) {
        if (minuscule_ascii(mot[i]) != modele_minuscule[i]) return false;
    }
    return true;
}

// `mot` : ce qui précède le point, depuis le dernier blanc. Le point qui le suit ne termine-t-il pas la phrase ?
constexpr bool point_d_abreviation(std::string_view mot) noexcept {
    while (!mot.empty() && (mot.front() == '(' || mot.front() == '[' || mot.front() == '"' || mot.front() == '\'')) {
        mot.remove_prefix(1);
    }
    if (mot.empty()) return false;
    // numéro de liste : 1 ou 2 chiffres
    if (mot.size() <= 2) {
        bool chiffres = true;
        for (char c : mot) chiffres = chiffres && c >= '0' && c <= '9';
        if (chiffres) return true;
    }
    // initiale : une seule majuscule
    if (mot.size() == 1 && mot[0] >= 'A' && mot[0] <= 'Z') return true;
    for (std::string_view a : ABREVIATIONS) {
        if (meme_mot_sans_casse(mot, a)) return true;
    }
    // forme à points internes : composantes de 1 à 2 lettres ASCII (« p.ex », « U.S.A », « i.e »)
    if (mot.find('.') != std::string_view::npos) {
        std::size_t lettres = 0;
        for (std::size_t i = 0; i <= mot.size(); ++i) {
            if (i == mot.size() || mot[i] == '.') {
                if (lettres == 0 || lettres > 2) return false;
                lettres = 0;
            } else if ((mot[i] >= 'a' && mot[i] <= 'z') || (mot[i] >= 'A' && mot[i] <= 'Z')) {
                ++lettres;
            } else {
                return false;
            }
        }
        return true;
    }
    return false;
}

// Reste-t-il une lettre ou un chiffre (ou un caractère non ponctuation) à dire ?
constexpr bool a_du_contenu(std::string_view s) noexcept {
    for (std::size_t i = 0; i < s.size();) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80U) {
            if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) return true;
            ++i;
            continue;
        }
        const std::size_t n = utf8::sequence_valide(s, i);
        if (n == 0) {
            ++i;
            continue;
        }
        const std::uint32_t cp = utf8::point_de_code(s, i, n);
        const bool ponctuation = cp < 0xC0U || (cp >= 0x2000U && cp <= 0x206FU);  // Latin-1 symboles, ponctuation générale
        if (!ponctuation) return true;
        i += n;
    }
    return false;
}

constexpr std::string_view rogner_blancs(std::string_view s) noexcept {
    while (!s.empty() && est_blanc_phrase(s.front())) s.remove_prefix(1);
    while (!s.empty() && est_blanc_phrase(s.back())) s.remove_suffix(1);
    return s;
}

}  // namespace detail

template <std::size_t LIMITE = 200>
class DecoupePhrases {
    static_assert(LIMITE >= 32, "limite trop petite pour une phrase");

public:
    // Ajoute un morceau de réponse (taille quelconque, même coupé au milieu d'un caractère UTF-8) ; appelle
    // `sur_phrase(std::string_view)` pour chaque phrase complète (vue valable pendant l'appel seulement).
    template <class F>
    constexpr void ajouter(std::string_view morceau, F&& sur_phrase) {
        while (!morceau.empty()) {
            std::size_t k = LIMITE - n_;  // > 0 : après chaque extraction il reste de la place
            if (k > morceau.size()) k = morceau.size();
            for (std::size_t i = 0; i < k; ++i) buf_[n_ + i] = morceau[i];
            n_ += k;
            morceau.remove_prefix(k);
            extraire(sur_phrase);
        }
    }

    // Fin de la réponse : émet ce qui reste (même sans ponctuation finale) et repart à zéro.
    template <class F>
    constexpr void terminer(F&& sur_phrase) {
        extraire(sur_phrase);
        emettre(sur_phrase, n_);
        n_ = 0;
    }

    constexpr std::size_t en_attente() const noexcept { return n_; }
    constexpr void remettre_a_zero() noexcept { n_ = 0; }

private:
    template <class F>
    constexpr void emettre(F& sur_phrase, std::size_t fin) {
        const std::string_view phrase = detail::rogner_blancs(std::string_view(buf_, fin));
        if (!phrase.empty() && detail::a_du_contenu(phrase)) sur_phrase(phrase);
    }

    // Retire les `k` premiers octets, puis les blancs qui suivent.
    constexpr void retirer(std::size_t k) noexcept {
        while (k < n_ && detail::est_blanc_phrase(buf_[k])) ++k;
        for (std::size_t i = k; i < n_; ++i) buf_[i - k] = buf_[i];
        n_ -= k;
    }

    // Cherche la première coupe naturelle. Rend vrai avec `fin` (fin de la phrase, exclue) et `suite` (premier
    // octet à garder). Faux = rien pour l'instant (ou une confirmation attend le prochain morceau).
    constexpr bool chercher_coupe(std::size_t& fin, std::size_t& suite) const noexcept {
        for (std::size_t i = 0; i < n_;) {
            const char c = buf_[i];
            if (c == '\n') {
                fin = i;
                suite = i + 1;
                return true;
            }
            if (c != '.' && c != '!' && c != '?' && !ellipse_en(i)) {
                ++i;
                continue;
            }
            // groupe de terminateurs : « ?! », « ... », « … »
            std::size_t j = i;
            while (j < n_) {
                if (buf_[j] == '.' || buf_[j] == '!' || buf_[j] == '?') {
                    ++j;
                } else if (ellipse_en(j)) {
                    j += 3;
                } else {
                    break;
                }
            }
            std::size_t k = j;  // fermants collés : " ' ) ] » ” ’
            while (k < n_) {
                const std::size_t f = fermant_en(k);
                if (f == 0) break;
                k += f;
            }
            if (k >= n_) return false;  // on ne sait pas encore ce qui suit : attendre le prochain morceau
            if (detail::est_blanc_phrase(buf_[k])) {
                const bool point_seul = (j == i + 1 && buf_[i] == '.');
                if (!(point_seul && point_d_abreviation_avant(i))) {
                    fin = k;
                    suite = k;
                    return true;
                }
            }
            i = k > i ? k : i + 1;
        }
        return false;
    }

    constexpr bool ellipse_en(std::size_t i) const noexcept {  // « … » = E2 80 A6
        return i + 3 <= n_ && static_cast<unsigned char>(buf_[i]) == 0xE2U && static_cast<unsigned char>(buf_[i + 1]) == 0x80U &&
               static_cast<unsigned char>(buf_[i + 2]) == 0xA6U;
    }

    // Longueur du fermant qui commence en `i` (0 si ce n'en est pas un).
    constexpr std::size_t fermant_en(std::size_t i) const noexcept {
        const char c = buf_[i];
        if (c == '"' || c == '\'' || c == ')' || c == ']') return 1;
        const unsigned char u = static_cast<unsigned char>(c);
        if (u == 0xC2U && i + 2 <= n_ && static_cast<unsigned char>(buf_[i + 1]) == 0xBBU) return 2;  // »
        if (u == 0xE2U && i + 3 <= n_ && static_cast<unsigned char>(buf_[i + 1]) == 0x80U) {
            const unsigned char t = static_cast<unsigned char>(buf_[i + 2]);
            if (t == 0x9DU || t == 0x99U) return 3;  // ” ’
        }
        return 0;
    }

    constexpr bool point_d_abreviation_avant(std::size_t i) const noexcept {
        std::size_t debut = i;
        while (debut > 0 && !detail::est_blanc_phrase(buf_[debut - 1])) --debut;
        return detail::point_d_abreviation(std::string_view(buf_ + debut, i - debut));
    }

    // Tampon plein sans coupe naturelle : où couper ? (toujours ≥ 1)
    constexpr std::size_t coupe_forcee() const noexcept {
        const std::size_t moitie = LIMITE / 2;
        for (std::size_t p = n_ - 1; p-- > moitie;) {  // p = n_-2 … moitie
            if ((buf_[p] == ',' || buf_[p] == ';' || buf_[p] == ':') && buf_[p + 1] == ' ') return p + 1;
        }
        for (std::size_t p = n_; p-- > moitie;) {
            if (buf_[p] == ' ') return p;
        }
        const std::size_t entier = utf8::coupe_sans_casser(std::string_view(buf_, n_), n_);
        return entier == 0 ? n_ : entier;
    }

    template <class F>
    constexpr void extraire(F& sur_phrase) {
        while (n_ > 0) {
            std::size_t fin = 0;
            std::size_t suite = 0;
            if (chercher_coupe(fin, suite)) {
                emettre(sur_phrase, fin);
                retirer(suite);
            } else if (n_ >= LIMITE) {
                const std::size_t c = coupe_forcee();
                emettre(sur_phrase, c);
                retirer(c);
            } else {
                break;
            }
        }
    }

    char buf_[LIMITE] = {};
    std::size_t n_ = 0;
};

}  // namespace ecran::assistant
