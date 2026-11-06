// Noyau pur : petits outils de texte de l'assistant vocal, partagés par flux_sse, requetes_openai, extrait_openai
// et decoupe_phrases. C++17 standard, aucun include ESP-IDF, aucune allocation, évaluable à la compilation.
//
//  - utf8 : validation stricte (séquences trop courtes, surrogates, sur-longues, > U+10FFFF refusés), décodage,
//    encodage, détection d'un caractère coupé en fin de tampon ;
//  - Tampon : écrivain sur un tableau fourni par l'appelant (rien n'est alloué ; débordement = drapeau, jamais de
//    dépassement ni de caractère UTF-8 coupé en deux par l'appelant s'il écrit des caractères entiers).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace ecran::assistant {

namespace utf8 {

constexpr bool est_continuation(char c) noexcept { return (static_cast<unsigned char>(c) & 0xC0U) == 0x80U; }

// Longueur attendue d'une séquence d'après son premier octet : 1 (ASCII), 2, 3, 4 ; 0 si l'octet ne peut pas
// commencer une séquence (continuation isolée, 0xC0/0xC1, 0xF5 et plus).
constexpr std::size_t longueur_attendue(char premier) noexcept {
    const unsigned b = static_cast<unsigned char>(premier);
    if (b < 0x80U) return 1;
    if (b >= 0xC2U && b <= 0xDFU) return 2;
    if (b >= 0xE0U && b <= 0xEFU) return 3;
    if (b >= 0xF0U && b <= 0xF4U) return 4;
    return 0;
}

// Longueur (1 à 4) de la séquence VALIDE qui commence à `pos`, 0 si elle est invalide ou incomplète.
constexpr std::size_t sequence_valide(std::string_view s, std::size_t pos) noexcept {
    if (pos >= s.size()) return 0;
    const std::size_t n = longueur_attendue(s[pos]);
    if (n == 0 || pos + n > s.size()) return 0;
    for (std::size_t i = 1; i < n; ++i) {
        if (!est_continuation(s[pos + i])) return 0;
    }
    if (n == 3) {
        const unsigned b0 = static_cast<unsigned char>(s[pos]);
        const unsigned b1 = static_cast<unsigned char>(s[pos + 1]);
        if (b0 == 0xE0U && b1 < 0xA0U) return 0;  // sur-longue
        if (b0 == 0xEDU && b1 >= 0xA0U) return 0;  // surrogate U+D800 à U+DFFF
    } else if (n == 4) {
        const unsigned b0 = static_cast<unsigned char>(s[pos]);
        const unsigned b1 = static_cast<unsigned char>(s[pos + 1]);
        if (b0 == 0xF0U && b1 < 0x90U) return 0;  // sur-longue
        if (b0 == 0xF4U && b1 >= 0x90U) return 0;  // au-delà de U+10FFFF
    }
    return n;
}

// Point de code de la séquence valide de `n` octets qui commence à `pos` (appeler après sequence_valide).
constexpr std::uint32_t point_de_code(std::string_view s, std::size_t pos, std::size_t n) noexcept {
    const std::uint32_t b0 = static_cast<unsigned char>(s[pos]);
    if (n == 1) return b0;
    std::uint32_t cp = n == 2 ? (b0 & 0x1FU) : n == 3 ? (b0 & 0x0FU) : (b0 & 0x07U);
    for (std::size_t i = 1; i < n; ++i) cp = (cp << 6) | (static_cast<unsigned char>(s[pos + i]) & 0x3FU);
    return cp;
}

// Écrit `cp` en UTF-8 dans `sortie` (4 octets au plus) ; rend le nombre d'octets, 0 si `cp` n'est pas un scalaire
// Unicode (surrogate ou > U+10FFFF).
constexpr std::size_t encoder(std::uint32_t cp, char* sortie) noexcept {
    if (cp < 0x80U) {
        sortie[0] = static_cast<char>(cp);
        return 1;
    }
    if (cp < 0x800U) {
        sortie[0] = static_cast<char>(0xC0U | (cp >> 6));
        sortie[1] = static_cast<char>(0x80U | (cp & 0x3FU));
        return 2;
    }
    if (cp >= 0xD800U && cp <= 0xDFFFU) return 0;
    if (cp < 0x10000U) {
        sortie[0] = static_cast<char>(0xE0U | (cp >> 12));
        sortie[1] = static_cast<char>(0x80U | ((cp >> 6) & 0x3FU));
        sortie[2] = static_cast<char>(0x80U | (cp & 0x3FU));
        return 3;
    }
    if (cp > 0x10FFFFU) return 0;
    sortie[0] = static_cast<char>(0xF0U | (cp >> 18));
    sortie[1] = static_cast<char>(0x80U | ((cp >> 12) & 0x3FU));
    sortie[2] = static_cast<char>(0x80U | ((cp >> 6) & 0x3FU));
    sortie[3] = static_cast<char>(0x80U | (cp & 0x3FU));
    return 4;
}

// Texte entièrement en UTF-8 valide ?
constexpr bool valide(std::string_view s) noexcept {
    for (std::size_t i = 0; i < s.size();) {
        const std::size_t n = sequence_valide(s, i);
        if (n == 0) return false;
        i += n;
    }
    return true;
}

// Plus grande longueur ≤ `n` qui ne coupe aucun caractère de `s[0, n)` : si le dernier caractère est incomplet
// (la fin du tampon tombe au milieu), rend la position de son premier octet ; sinon `n`.
constexpr std::size_t coupe_sans_casser(std::string_view s, std::size_t n) noexcept {
    if (n > s.size()) n = s.size();
    if (n == 0) return 0;
    std::size_t tete = n - 1;  // octet de tête du dernier caractère : on recule d'au plus 3 octets de continuation
    while (tete > 0 && n - tete < 4 && est_continuation(s[tete])) --tete;
    const std::size_t attendu = longueur_attendue(s[tete]);
    if (attendu == 0) return n;  // octet invalide : ce n'est pas un caractère coupé, on ne le retient pas
    return tete + attendu <= n ? n : tete;
}

}  // namespace utf8

// Écrivain sur un tableau de l'appelant. Une écriture qui ne tient pas en entier met `deborde()` à vrai et
// n'écrit RIEN (ni cette pièce ni les suivantes) : on ne produit jamais un texte tronqué sans le savoir.
class Tampon {
public:
    constexpr Tampon(char* donnees, std::size_t capacite) noexcept : d_(donnees), cap_(capacite) {}

    constexpr bool ecrire(char c) noexcept {
        if (deborde_ || n_ >= cap_) {
            deborde_ = true;
            return false;
        }
        d_[n_++] = c;
        return true;
    }

    constexpr bool ecrire(std::string_view s) noexcept {
        if (deborde_ || s.size() > cap_ - n_) {
            deborde_ = true;
            return false;
        }
        for (char c : s) d_[n_++] = c;
        return true;
    }

    constexpr bool ecrire_decimal(std::uint64_t v) noexcept {
        char tmp[20] = {};
        std::size_t k = 0;
        do {
            tmp[k++] = static_cast<char>('0' + static_cast<int>(v % 10U));
            v /= 10U;
        } while (v != 0);
        if (deborde_ || k > cap_ - n_) {
            deborde_ = true;
            return false;
        }
        while (k > 0) d_[n_++] = tmp[--k];
        return true;
    }

    constexpr std::string_view vue() const noexcept { return std::string_view(d_, n_); }
    constexpr std::size_t taille() const noexcept { return n_; }
    constexpr std::size_t capacite() const noexcept { return cap_; }
    constexpr bool deborde() const noexcept { return deborde_; }
    constexpr bool ok() const noexcept { return !deborde_; }

    constexpr void vider() noexcept {
        n_ = 0;
        deborde_ = false;
    }

private:
    char* d_;
    std::size_t cap_;
    std::size_t n_ = 0;
    bool deborde_ = false;
};

}  // namespace ecran::assistant
