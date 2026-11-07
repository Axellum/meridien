// Noyau pur : ce que la PASTILLE de l'écran principal dit d'une alerte officielle en vigueur. C++17 standard, aucun include ESP-IDF ni
// LVGL, aucune allocation, évaluable à la compilation (test/host/test_vigie_bandeau.cpp). S'appuie sur vigie_vue.hpp (quelles alertes
// sont en vigueur) et vigie_textes.hpp (les mots de Vigie quand l'émetteur n'a rien dit de dessinable).
//
// La pastille ne parle que de l'alerte la PLUS GRAVE en vigueur (vigie::Vue::indices[0]) ; les autres sont comptées (`autres`, « +2 ») et
// lues sur la page des alertes, qu'un appui sur la pastille ouvre. Elle n'existe QUE quand une alerte est en vigueur : rien d'autre
// n'est jamais affiché à cette place.
//
// Les mots sont ceux de l'ÉMETTEUR (titre du texte dans la langue de l'écran, sinon en anglais, sinon le premier ; à défaut son `event`,
// puis le phénomène d'origine), jamais réécrits. Une seule chose est changée : les retours à la ligne et les tabulations deviennent des
// espaces (la pastille tient sur une ligne ; la page, elle, garde les retours à la ligne de l'émetteur), et les espaces en tête, en
// queue ou en double sont retirés. Le texte est tenu d'un seul tenant dans `texte` : s'il ne tient pas dans la pastille, c'est l'interface
// qui le coupe avec « … » (ui_principal.cpp), pas ce fichier.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/i18n.hpp"
#include "ecran/vigie_alertes.hpp"
#include "ecran/vigie_textes.hpp"
#include "ecran/vigie_vue.hpp"

namespace ecran::vigie {

inline constexpr std::size_t BANDEAU_TEXTE_CAP = TITRE_CAP; // octets, zéro final compris

struct Bandeau {
    bool present = false;
    Niveau niveau = Niveau::Inconnu;    // couleur de la pastille (couleur_officielle du thème)
    std::uint8_t autres = 0;            // alertes en vigueur en plus de celle qui est montrée
    char texte[BANDEAU_TEXTE_CAP] = {}; // une ligne, UTF-8
    std::uint32_t empreinte = 0;        // vigie::empreinte : change quand ce que la pastille ou la page montrent change
};

namespace detail {

// Copie `s` dans `sortie` : retours à la ligne et tabulations -> espace, espaces de bord et doubles retirés. Rend la longueur écrite.
constexpr std::size_t une_ligne(std::string_view s, char *sortie, std::size_t capacite) noexcept {
    if (sortie == nullptr || capacite == 0)
        return 0;
    std::size_t n = 0;
    bool espace_en_attente = false;
    for (const char c : s) {
        const bool blanc = c == ' ' || c == '\n' || c == '\r' || c == '\t';
        if (blanc) {
            espace_en_attente = n > 0;
            continue;
        }
        const std::size_t besoin = (espace_en_attente ? 1 : 0) + 1;
        if (n + besoin + 1 > capacite)
            break; // pas la place du caractère et du zéro : on s'arrête là (le texte a déjà la taille d'un titre, cela n'arrive pas)
        if (espace_en_attente)
            sortie[n++] = ' ';
        espace_en_attente = false;
        sortie[n++] = c;
    }
    sortie[n] = '\0';
    return n;
}

} // namespace detail

constexpr Bandeau composer_bandeau(const Alertes &a, const Vue &v, i18n::Langue langue) noexcept {
    Bandeau b;
    if (v.nombre == 0 || v.indices[0] >= a.nombre)
        return b;
    const Alerte &x = a.alertes[v.indices[0]];
    b.present = true;
    b.niveau = x.niveau;
    b.autres = static_cast<std::uint8_t>(v.nombre - 1);
    const std::string_view code_langue = i18n::code(langue);
    const Titre titre = titre_de(x, code_langue);
    std::size_t n = 0;
    if (titre.origine != OrigineTitre::Aucune)
        n = detail::une_ligne(titre.texte, b.texte, sizeof b.texte);
    if (n == 0)
        libelle_derive(x.phenomene, x.niveau, langue, b.texte,
                       sizeof b.texte); // les mots de Vigie, dans la langue de l'écran
    // L'empreinte de la page (tout ce qu'elle montre : le niveau et le texte de chaque alerte en vigueur, et donc leur nombre, d'où le niveau
    // et le « +N » de la pastille) puis ce que la pastille ajoute : son texte final (les mots de Vigie changent avec la langue de l'écran
    // même quand le texte de l'émetteur reste le même).
    std::uint32_t h = empreinte(a, v, code_langue);
    const auto octet = [&h](std::uint32_t o) { h = (h ^ (o & 0xFFu)) * 16777619u; };
    for (std::size_t i = 0; b.texte[i] != '\0' && i < sizeof b.texte; ++i)
        octet(static_cast<unsigned char>(b.texte[i]));
    octet(0);
    b.empreinte = h;
    return b;
}

} // namespace ecran::vigie
