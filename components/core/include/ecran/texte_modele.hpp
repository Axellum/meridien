// Noyau pur : remplacement des marqueurs nommés {nom} d'un texte traduit (« Connecté à « {ssid} » »). Les traductions gardent les
// mêmes marqueurs que la clé française, mais pas forcément dans le même ordre (c'est le but des noms, contrairement à %s).
// Un marqueur inconnu reste tel quel (visible, donc facile à repérer) ; la sortie est toujours terminée par un zéro et ne coupe
// jamais un caractère UTF-8 en deux.
// C++17 standard, aucun include ESP-IDF ni LVGL, évaluable à la compilation.
#pragma once

#include <cstddef>
#include <string_view>

namespace ecran::texte {

struct Parametre {
    std::string_view nom; // sans accolades : « ssid »
    std::string_view valeur;
};

namespace detail {

// Longueur du caractère UTF-8 qui commence par `lead` (1 pour un octet isolé ou invalide).
constexpr std::size_t longueur_caractere(unsigned char lead) noexcept {
    return lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
}

// Écrit `s` dans `sortie` à partir de `pos`, caractère entier par caractère entier, sans dépasser `capacite - 1` octets : un
// caractère qui ne tient pas entièrement n'est pas écrit du tout. Rend la nouvelle position.
constexpr std::size_t ajouter(char *sortie, std::size_t capacite, std::size_t pos, std::string_view s) noexcept {
    std::size_t i = 0;
    while (i < s.size()) {
        std::size_t n = longueur_caractere(static_cast<unsigned char>(s[i]));
        if (i + n > s.size())
            n = s.size() - i;
        if (pos + n + 1 > capacite)
            break;
        for (std::size_t k = 0; k < n; ++k)
            sortie[pos++] = s[i + k];
        i += n;
    }
    return pos;
}

} // namespace detail

// Écrit `modele` dans `sortie` (taille `capacite`, zéro final compris) en remplaçant chaque {nom} connu. Rend le nombre d'octets
// écrits, zéro final exclu.
constexpr std::size_t remplir(std::string_view modele, const Parametre *parametres, std::size_t nombre, char *sortie,
                              std::size_t capacite) noexcept {
    if (capacite == 0)
        return 0;
    std::size_t pos = 0;
    std::size_t i = 0;
    while (i < modele.size()) {
        if (modele[i] == '{') {
            const std::size_t fin = modele.find('}', i + 1);
            if (fin != std::string_view::npos) {
                const std::string_view nom = modele.substr(i + 1, fin - i - 1);
                bool trouve = false;
                for (std::size_t k = 0; k < nombre; ++k) {
                    if (parametres[k].nom == nom) {
                        pos = detail::ajouter(sortie, capacite, pos, parametres[k].valeur);
                        trouve = true;
                        break;
                    }
                }
                if (trouve) {
                    i = fin + 1;
                    continue;
                }
            }
        }
        const std::size_t n = detail::longueur_caractere(static_cast<unsigned char>(modele[i]));
        pos = detail::ajouter(sortie, capacite, pos, modele.substr(i, n));
        i += n;
    }
    sortie[pos] = '\0';
    return pos;
}

} // namespace ecran::texte
