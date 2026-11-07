// Tests du noyau pur : sécurité de la NVS (nvs_securite.hpp) — lecture brute des pages, classement du contenu d'une partition et table de
// décision « que faire avant de l'ouvrir ». La migration (instantané, purge, écriture chiffrée, relecture) est dans test_nvs_migration.cpp.
//
// Les pages viennent de l'outil officiel d'ESP-IDF (test/host/capturer_nvs.py : `generate` pour le clair, `encrypt` avec une clé
// jetable pour le chiffré) et les nombres attendus sont calculés par Python (struct, zlib), pas par le noyau : voir
// test/host/donnees/LISEZMOI_nvs.md. Rien ici n'a tourné sur une carte : la NVS réelle d'une carte n'a jamais été lue par ce code.
#include <array>
#include <cstddef>
#include <cstdint>

#include "ecran/nvs_securite.hpp"
#include "mini.hpp"

namespace n = ecran::nvs_securite;
using n::Contenu;
using n::Decision;
using n::Mode;

namespace {

#include "donnees/attendu_nvs.inc"

constexpr std::uint8_t CLAIR[n::TAILLE_PAGE] = {
#include "donnees/nvs_clair.inc"
};
constexpr std::uint8_t CHIFFRE[n::TAILLE_PAGE] = {
#include "donnees/nvs_chiffre.inc"
};

using Page = std::array<std::uint8_t, n::TAILLE_PAGE>;
using Trois = std::array<std::uint8_t, 3 * n::TAILLE_PAGE>;

constexpr Page copie(const std::uint8_t (&p)[n::TAILLE_PAGE]) {
    Page a{};
    for (std::size_t i = 0; i < a.size(); ++i) {
        a[i] = p[i];
    }
    return a;
}

constexpr Page page_a_ff() {
    Page a{};
    for (auto &b : a) {
        b = 0xFF;
    }
    return a;
}

// Une partition de trois pages (comme celle que produit l'outil) : les pages données d'abord, le reste vierge (0xFF).
constexpr Trois trois(const std::uint8_t *p0 = nullptr, const std::uint8_t *p1 = nullptr) {
    Trois t{};
    for (auto &b : t) {
        b = 0xFF;
    }
    for (std::size_t i = 0; p0 != nullptr && i < n::TAILLE_PAGE; ++i) {
        t[i] = p0[i];
    }
    for (std::size_t i = 0; p1 != nullptr && i < n::TAILLE_PAGE; ++i) {
        t[n::TAILLE_PAGE + i] = p1[i];
    }
    return t;
}

constexpr bool contient(const std::uint8_t *page, const char *texte) {
    std::size_t m = 0;
    while (texte[m] != '\0') {
        ++m;
    }
    for (std::size_t i = 0; i + m <= n::TAILLE_PAGE; ++i) {
        std::size_t k = 0;
        while (k < m && page[i + k] == static_cast<std::uint8_t>(texte[k])) {
            ++k;
        }
        if (k == m) {
            return true;
        }
    }
    return false;
}

// Écrit une entrée de 32 octets valide (somme de contrôle juste) à la place `rang` d'une page, dans l'état `etat`.
constexpr void poser_entree(Page &page, std::size_t rang, std::uint8_t span, unsigned etat) {
    std::uint8_t e[n::TAILLE_ENTREE] = {};
    e[0] = 1;    // index d'espace
    e[1] = 0x14; // type : entier signé 32 bits
    e[2] = span;
    e[3] = 0xFF; // pas de morceau
    for (std::size_t i = 8; i < n::TAILLE_ENTREE; ++i) {
        e[i] = static_cast<std::uint8_t>(0x41 + i);
    }
    std::uint8_t u[28] = {};
    for (std::size_t i = 0; i < 4; ++i) {
        u[i] = e[i];
    }
    for (std::size_t i = 8; i < 32; ++i) {
        u[i - 4] = e[i];
    }
    const std::uint32_t crc = n::crc_nvs(u, sizeof(u));
    e[4] = static_cast<std::uint8_t>(crc);
    e[5] = static_cast<std::uint8_t>(crc >> 8);
    e[6] = static_cast<std::uint8_t>(crc >> 16);
    e[7] = static_cast<std::uint8_t>(crc >> 24);
    for (std::size_t i = 0; i < n::TAILLE_ENTREE; ++i) {
        page[n::DECALAGE_ENTREES + rang * n::TAILLE_ENTREE + i] = e[i];
    }
    const std::size_t octet = n::DECALAGE_TABLE + rang / 4;
    const unsigned decalage = static_cast<unsigned>(2 * (rang % 4));
    page[octet] = static_cast<std::uint8_t>((page[octet] & ~(3u << decalage)) | (etat << decalage));
}

// Une page formatée sans aucune entrée : l'en-tête de la page en clair, tout le reste à 0xFF.
constexpr Page page_formatee_sans_entree() {
    Page a = page_a_ff();
    for (std::size_t i = 0; i < n::DECALAGE_TABLE; ++i) {
        a[i] = CLAIR[i];
    }
    return a;
}

// ---- format -----------------------------------------------------------------------------------------------------------------

constexpr int crc_de_nvs_n_est_pas_le_crc32_usuel() {
    const std::uint8_t chiffres[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    VERIFIER(n::crc_nvs(chiffres, sizeof(chiffres)) == ATTENDU_CRC_123456789);
    VERIFIER(n::crc_nvs(chiffres, sizeof(chiffres)) != 0xCBF43926u); // le CRC-32 usuel de la même suite
    VERIFIER(n::crc_nvs(chiffres, 0) == 0xFFFFFFFFu);                // suite vide : zlib.crc32(b"", 0xFFFFFFFF)
    return 0;
}

constexpr int constantes_du_format_lues_dans_idf() {
    VERIFIER(n::TAILLE_PAGE == 4096 && n::TAILLE_ENTREE == 32 && n::ENTREES_PAR_PAGE == 126);
    VERIFIER(n::DECALAGE_TABLE == 32 && n::DECALAGE_ENTREES == 64);
    VERIFIER(n::DECALAGE_ENTREES + n::ENTREES_PAR_PAGE * n::TAILLE_ENTREE == n::TAILLE_PAGE); // 64 + 126 × 32 = 4096
    VERIFIER(n::PAGE_ACTIVE == 0xFFFFFFFEu && n::PAGE_PLEINE == 0xFFFFFFFCu);
    VERIFIER(n::PAGE_EN_LIBERATION == 0xFFFFFFF8u && n::PAGE_ABIMEE == 0xFFFFFFF0u);
    VERIFIER(n::ENTREE_EFFACEE == 0 && n::ENTREE_ECRITE == 2);
    return 0;
}

constexpr int etat_d_une_entree_est_sur_deux_bits() {
    Page p = page_a_ff();
    // octet 0 de la table : entrées 0 à 3 = 11, 10, 00, 11 (vide, écrite, effacée, vide)
    p[n::DECALAGE_TABLE] = static_cast<std::uint8_t>(3 | (2 << 2) | (0 << 4) | (3 << 6));
    VERIFIER(n::etat_entree(p.data(), 0) == 3 && n::etat_entree(p.data(), 1) == 2);
    VERIFIER(n::etat_entree(p.data(), 2) == 0 && n::etat_entree(p.data(), 3) == 3);
    p[n::DECALAGE_TABLE + 1] = 0xFE; // entrée 4 = 10
    VERIFIER(n::etat_entree(p.data(), 4) == 2 && n::etat_entree(p.data(), 5) == 3);
    p[n::DECALAGE_TABLE + 31] = 0xF3; // entrée 124 = 11, entrée 125 = 00
    VERIFIER(n::etat_entree(p.data(), 124) == 3 && n::etat_entree(p.data(), 125) == 0);
    return 0;
}

constexpr int en_tete_de_l_outil_est_valide_meme_chiffre() {
    VERIFIER(n::entete_valide(CLAIR));
    VERIFIER(n::entete_valide(CHIFFRE)); // l'en-tête n'est jamais chiffré
    Page p = copie(CLAIR);
    p[4] ^= 1; // numéro de séquence modifié : la somme de contrôle ne tombe plus juste
    VERIFIER(!n::entete_valide(p.data()));
    p = copie(CLAIR);
    p[0] = 0x00; // état « invalide »
    p[1] = 0x00;
    p[2] = 0x00;
    p[3] = 0x00;
    VERIFIER(!n::entete_valide(p.data()));
    p = copie(CLAIR);
    p[28] ^= 0x80; // somme de contrôle de l'en-tête modifiée
    VERIFIER(!n::entete_valide(p.data()));
    return 0;
}

constexpr int une_entree_chiffree_n_est_pas_lisible() {
    VERIFIER(n::entree_lisible(CLAIR + n::DECALAGE_ENTREES));
    VERIFIER(!n::entree_lisible(CHIFFRE + n::DECALAGE_ENTREES));
    Page p = copie(CLAIR);
    p[n::DECALAGE_ENTREES + 20] ^= 1; // un bit de la clé de l'entrée
    VERIFIER(!n::entree_lisible(p.data() + n::DECALAGE_ENTREES));
    p = copie(CLAIR);
    p[n::DECALAGE_ENTREES + 5] ^= 1; // un bit de la somme de contrôle elle-même
    VERIFIER(!n::entree_lisible(p.data() + n::DECALAGE_ENTREES));
    return 0;
}

// ---- classement des pages de l'outil officiel -------------------------------------------------------------------------------

constexpr int page_vierge_est_vide() {
    const Page p = page_a_ff();
    const n::Bilan b = n::bilan_page(p.data());
    VERIFIER(b.pages_vides == 1 && b.pages_formatees == 0 && b.pages_inconnues == 0);
    VERIFIER(b.entrees_lisibles == 0 && b.entrees_illisibles == 0 && b.restes_lisibles == 0);
    VERIFIER(n::classer(b) == Contenu::Vide);
    return 0;
}

constexpr int page_en_clair_de_l_outil_officiel() {
    const n::Bilan b = n::bilan_page(CLAIR);
    VERIFIER(b.pages_formatees == 1 && b.pages_vides == 0 && b.pages_inconnues == 0);
    VERIFIER(b.entrees_lisibles == ATTENDU_CLAIR_LISIBLES); // 4 espaces + 3 textes + 2 entiers + 2 entrées de blob = 11
    VERIFIER(b.entrees_illisibles == ATTENDU_CLAIR_ILLISIBLES && b.restes_lisibles == 0);
    VERIFIER(n::classer(b) == Contenu::Claire);
    return 0;
}

constexpr int page_chiffree_de_l_outil_officiel() {
    const n::Bilan b = n::bilan_page(CHIFFRE);
    VERIFIER(b.pages_formatees == 1 && b.pages_vides == 0 && b.pages_inconnues == 0);
    VERIFIER(b.entrees_lisibles == ATTENDU_CHIFFRE_LISIBLES);     // zéro : aucune entrée ne se lit sans la clé
    VERIFIER(b.entrees_illisibles == ATTENDU_CHIFFRE_ILLISIBLES); // les 15 entrées écrites, comptées une à une
    VERIFIER(b.restes_lisibles == 0);
    VERIFIER(n::classer(b) == Contenu::Illisible);
    return 0;
}

constexpr int partition_de_trois_pages_comme_l_outil_la_produit() {
    const Trois claire = trois(CLAIR);
    n::Bilan b = n::bilan_partition(claire.data(), claire.size());
    VERIFIER(b.pages_formatees == 1 && b.pages_vides == 2 && b.pages_inconnues == 0);
    VERIFIER(b.entrees_lisibles == ATTENDU_CLAIR_LISIBLES && n::classer(b) == Contenu::Claire);
    const Trois chiffree = trois(CHIFFRE);
    b = n::bilan_partition(chiffree.data(), chiffree.size());
    VERIFIER(b.pages_formatees == 1 && b.pages_vides == 2);
    VERIFIER(b.entrees_illisibles == ATTENDU_CHIFFRE_ILLISIBLES && n::classer(b) == Contenu::Illisible);
    const Trois vide = trois();
    b = n::bilan_partition(vide.data(), vide.size());
    VERIFIER(b.pages_vides == 3 && n::classer(b) == Contenu::Vide);
    return 0;
}

constexpr int un_secret_efface_reste_lisible_dans_la_flash() {
    Page p = copie(CLAIR);
    for (std::size_t i = 0; i < 32;
         ++i) { // « effacement » de NVS : la table d'états passe à zéro, rien d'autre ne change
        p[n::DECALAGE_TABLE + i] = 0x00;
    }
    const n::Bilan b = n::bilan_page(p.data());
    VERIFIER(b.entrees_lisibles == 0 && b.entrees_illisibles == 0);
    VERIFIER(b.restes_lisibles == ATTENDU_EFFACE_RESTES);
    VERIFIER(n::classer(b) == Contenu::SansDonnee);
    // et le mot de passe est toujours là, octet pour octet, dans la page « effacée »
    VERIFIER(contient(p.data(), "essai-mot-de-passe-0001"));
    VERIFIER(contient(p.data(), "essai-cle-0002"));
    // le total d'une partition additionne aussi les restes
    const Trois t = trois(p.data());
    const n::Bilan total = n::bilan_partition(t.data(), t.size());
    VERIFIER(total.restes_lisibles == ATTENDU_EFFACE_RESTES && total.pages_formatees == 1 && total.pages_vides == 2);
    VERIFIER(n::classer(total) == Contenu::SansDonnee);
    return 0;
}

constexpr int page_formatee_sans_entree_est_sans_donnee() {
    const Page p = page_formatee_sans_entree();
    const n::Bilan b = n::bilan_page(p.data());
    VERIFIER(b.pages_formatees == 1 && b.pages_vides == 0 && b.pages_inconnues == 0);
    VERIFIER(b.entrees_lisibles == 0 && b.entrees_illisibles == 0 && b.restes_lisibles == 0);
    VERIFIER(n::classer(b) == Contenu::SansDonnee);
    return 0;
}

constexpr int clair_et_chiffre_ensemble_sont_melanges() {
    const Trois t = trois(CLAIR, CHIFFRE);
    const n::Bilan b = n::bilan_partition(t.data(), t.size());
    VERIFIER(b.entrees_lisibles == ATTENDU_CLAIR_LISIBLES && b.entrees_illisibles == ATTENDU_CHIFFRE_ILLISIBLES);
    VERIFIER(n::classer(b) == Contenu::Melangee);
    return 0;
}

constexpr int une_seule_entree_abimee_dans_du_clair_rend_la_partition_melangee() {
    Page p = copie(CLAIR);
    p[n::DECALAGE_ENTREES + 3 * n::TAILLE_ENTREE + 12] ^= 0x10; // un bit dans la clé de la quatrième entrée
    const n::Bilan b = n::bilan_page(p.data());
    VERIFIER(b.entrees_illisibles >= 1 && b.entrees_lisibles >= 1);
    VERIFIER(n::classer(b) == Contenu::Melangee); // jamais « claire » : on ne l'ouvre pas pour qu'elle s'efface
    return 0;
}

constexpr int page_etrangere_ou_a_demi_vierge_est_inconnue() {
    Page p{};
    for (auto &o : p) {
        o = 0x5A;
    }
    n::Bilan b = n::bilan_page(p.data());
    VERIFIER(b.pages_inconnues == 1 && b.pages_vides == 0 && b.pages_formatees == 0);
    VERIFIER(n::classer(b) == Contenu::Inconnue);
    p = page_a_ff();
    p[n::TAILLE_PAGE - 1] = 0x00; // un seul octet qui n'est pas 0xFF : ce n'est pas une page vierge
    b = n::bilan_page(p.data());
    VERIFIER(b.pages_vides == 0 && b.pages_inconnues == 1 && n::classer(b) == Contenu::Inconnue);
    p = copie(CLAIR);
    p[5] ^= 0xFF; // en-tête abîmé
    b = n::bilan_page(p.data());
    VERIFIER(b.pages_inconnues == 1 && b.entrees_lisibles == 0 && n::classer(b) == Contenu::Inconnue);
    return 0;
}

constexpr int une_page_inconnue_l_emporte_sur_tout() {
    Page etrangere{};
    for (auto &o : etrangere) {
        o = 0x5A;
    }
    const Trois t = trois(CLAIR, etrangere.data());
    const n::Bilan b = n::bilan_partition(t.data(), t.size());
    VERIFIER(b.entrees_lisibles == ATTENDU_CLAIR_LISIBLES && b.pages_inconnues == 1);
    VERIFIER(n::classer(b) == Contenu::Inconnue);
    return 0;
}

constexpr int taille_nulle_ou_incomplete_n_est_jamais_vide() {
    const Trois t = trois();
    VERIFIER(n::classer(n::bilan_partition(nullptr, n::TAILLE_PAGE)) == Contenu::Inconnue);
    VERIFIER(n::classer(n::bilan_partition(t.data(), 0)) == Contenu::Inconnue);
    VERIFIER(n::classer(n::bilan_partition(t.data(), n::TAILLE_PAGE - 1)) == Contenu::Inconnue);
    VERIFIER(n::classer(n::bilan_partition(t.data(), n::TAILLE_PAGE + 1)) == Contenu::Inconnue);
    VERIFIER(n::classer(n::bilan_partition(t.data(), n::TAILLE_PAGE)) == Contenu::Vide);
    return 0;
}

constexpr int entree_de_span_nul_n_arrete_pas_la_lecture() {
    Page p = page_formatee_sans_entree();
    poser_entree(p, 0, 0, n::ENTREE_ECRITE); // span 0 : on avance d'une seule entrée, sinon la boucle ne finirait pas
    poser_entree(p, 1, 1, n::ENTREE_ECRITE);
    const n::Bilan b = n::bilan_page(p.data());
    VERIFIER(b.entrees_lisibles == 2 && b.entrees_illisibles == 0 && b.restes_lisibles == 0);
    return 0;
}

constexpr int entree_de_span_trois_cache_les_deux_suivantes() {
    // Les deux entrées qui suivent sont la valeur d'un texte, pas des entrées : leurs octets quelconques ne comptent pas comme
    // « illisibles ».
    Page p = page_formatee_sans_entree();
    poser_entree(p, 0, 3, n::ENTREE_ECRITE);
    for (std::size_t r = 1; r <= 2; ++r) {
        for (std::size_t i = 0; i < n::TAILLE_ENTREE; ++i) {
            p[n::DECALAGE_ENTREES + r * n::TAILLE_ENTREE + i] = static_cast<std::uint8_t>('a' + i);
        }
        p[n::DECALAGE_TABLE + r / 4] = static_cast<std::uint8_t>(p[n::DECALAGE_TABLE + r / 4] & ~(3u << (2 * (r % 4))));
        p[n::DECALAGE_TABLE + r / 4] = static_cast<std::uint8_t>(p[n::DECALAGE_TABLE + r / 4] | (2u << (2 * (r % 4))));
    }
    const n::Bilan b = n::bilan_page(p.data());
    VERIFIER(b.entrees_lisibles == 1 && b.entrees_illisibles == 0);
    return 0;
}

constexpr int entree_effacee_de_span_nul_n_est_comptee_qu_une_fois() {
    Page p = page_formatee_sans_entree();
    poser_entree(p, 0, 1, n::ENTREE_EFFACEE);
    poser_entree(p, 1, 1, n::ENTREE_EFFACEE);
    const n::Bilan b = n::bilan_page(p.data());
    VERIFIER(b.restes_lisibles == 2 && b.entrees_lisibles == 0 && b.entrees_illisibles == 0);
    VERIFIER(n::classer(b) == Contenu::SansDonnee);
    return 0;
}

// ---- table de décision ------------------------------------------------------------------------------------------------------

constexpr Contenu TOUS[] = {Contenu::Vide,      Contenu::SansDonnee, Contenu::Claire,
                            Contenu::Illisible, Contenu::Melangee,   Contenu::Inconnue};

constexpr n::Situation situation(Contenu c, int bits) {
    n::Situation s;
    s.contenu = c;
    s.restes = (bits & 1) != 0;
    s.mode = (bits & 2) != 0 ? Mode::Chiffre : Mode::Clair;
    s.cle_existe = (bits & 4) != 0;
    s.cle_essayee = (bits & 8) != 0;
    s.ordre = (bits & 16) != 0;
    return s;
}

constexpr int mode_clair_ouvre_ce_qui_se_lit_sans_cle() {
    for (const Contenu c : {Contenu::Vide, Contenu::SansDonnee, Contenu::Claire}) {
        for (int bits = 0; bits < 32; bits += 1) {
            if ((bits & 2) != 0) {
                continue; // mode chiffré : autre test
            }
            VERIFIER(n::decider(situation(c, bits)) == Decision::Ouvrir);
        }
    }
    return 0;
}

constexpr int mode_clair_refuse_une_partition_chiffree() {
    for (int bits = 0; bits < 32; ++bits) {
        if ((bits & 2) != 0) {
            continue;
        }
        // clé existante, clé essayée, ordre donné : rien n'y change, ouvrir en clair effacerait tout
        VERIFIER(n::decider(situation(Contenu::Illisible, bits)) == Decision::Refuser);
    }
    return 0;
}

constexpr int partition_melangee_ou_etrangere_est_toujours_refusee() {
    for (int bits = 0; bits < 32; ++bits) {
        VERIFIER(n::decider(situation(Contenu::Melangee, bits)) == Decision::Refuser);
        VERIFIER(n::decider(situation(Contenu::Inconnue, bits)) == Decision::Refuser);
    }
    return 0;
}

constexpr int mode_chiffre_sans_cle_n_ouvre_jamais() {
    for (const Contenu c : {Contenu::Vide, Contenu::SansDonnee, Contenu::Claire, Contenu::Illisible}) {
        for (int bits = 0; bits < 32; ++bits) {
            if ((bits & 2) == 0 || (bits & 4) != 0) {
                continue; // seulement : mode chiffré, clé absente
            }
            // même avec l'ordre d'Axel et une clé « essayée » : ouvrir créerait la clé (eFuse brûlé), seul Axel le fait, à la main
            VERIFIER(n::decider(situation(c, bits)) == Decision::AttendreCle);
        }
    }
    return 0;
}

constexpr int mode_chiffre_partition_vierge_s_ouvre() {
    for (int bits = 0; bits < 32; ++bits) {
        if ((bits & 2) == 0 || (bits & 4) == 0) {
            continue;
        }
        VERIFIER(n::decider(situation(Contenu::Vide, bits)) == Decision::Ouvrir);
    }
    return 0;
}

constexpr int mode_chiffre_du_clair_attend_l_ordre_puis_migre() {
    const int base = 2 | 4; // chiffré, clé existante
    for (int extra : {0, 1, 8}) {
        VERIFIER(n::decider(situation(Contenu::Claire, base | extra)) == Decision::AttendreOrdre);
        VERIFIER(n::decider(situation(Contenu::Claire, base | extra | 16)) == Decision::Migrer);
    }
    return 0;
}

constexpr int mode_chiffre_sans_donnee_purge_les_restes_sans_ordre() {
    const int base = 2 | 4;
    for (int extra : {0, 8, 16, 24}) {
        VERIFIER(n::decider(situation(Contenu::SansDonnee, base | extra | 1)) ==
                 Decision::Purger); // des restes : on purge
        VERIFIER(n::decider(situation(Contenu::SansDonnee, base | extra)) ==
                 Decision::Ouvrir); // pas de restes : on ouvre
    }
    return 0;
}

constexpr int mode_chiffre_illisible_exige_une_cle_essayee() {
    const int base = 2 | 4;
    for (int extra : {0, 1, 16, 17}) { // restes et ordre dans toutes leurs combinaisons
        VERIFIER(n::decider(situation(Contenu::Illisible, base | extra)) == Decision::Refuser); // clé non essayée
    }
    const int essayee = base | 8;
    VERIFIER(n::decider(situation(Contenu::Illisible, essayee)) == Decision::Ouvrir);
    VERIFIER(n::decider(situation(Contenu::Illisible, essayee | 1)) == Decision::Ouvrir);  // des restes, pas d'ordre
    VERIFIER(n::decider(situation(Contenu::Illisible, essayee | 16)) == Decision::Ouvrir); // un ordre, pas de restes
    VERIFIER(n::decider(situation(Contenu::Illisible, essayee | 1 | 16)) == Decision::Migrer);
    return 0;
}

constexpr int invariants_sur_toutes_les_situations() {
    for (const Contenu c : TOUS) {
        for (int bits = 0; bits < 32; ++bits) {
            const n::Situation s = situation(c, bits);
            const Decision d = n::decider(s);
            const bool vivant_lisible_seulement_avec_cle = c == Contenu::Illisible;
            if (d == Decision::Ouvrir) {
                VERIFIER(c != Contenu::Melangee && c != Contenu::Inconnue);
                if (s.mode == Mode::Clair) {
                    VERIFIER(!vivant_lisible_seulement_avec_cle);
                } else {
                    VERIFIER(s.cle_existe);                          // ouvrir ne crée jamais la clé
                    VERIFIER(c != Contenu::Claire);                  // jamais de secret en clair sous un mode chiffré
                    VERIFIER(c != Contenu::SansDonnee || !s.restes); // ni de reste en clair laissé là
                    VERIFIER(!vivant_lisible_seulement_avec_cle ||
                             s.cle_essayee); // une clé jamais essayée n'ouvre rien
                }
            }
            if (d == Decision::Migrer) {
                VERIFIER(s.ordre && s.mode == Mode::Chiffre && s.cle_existe);
                VERIFIER(c == Contenu::Claire || (c == Contenu::Illisible && s.cle_essayee && s.restes));
            }
            if (d == Decision::Purger) {
                VERIFIER(s.mode == Mode::Chiffre && s.cle_existe && c == Contenu::SansDonnee && s.restes);
            }
            if (d == Decision::AttendreCle) {
                VERIFIER(s.mode == Mode::Chiffre && !s.cle_existe);
            }
            if (d == Decision::AttendreOrdre) {
                VERIFIER(!s.ordre && c == Contenu::Claire);
            }
        }
    }
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(crc_de_nvs_n_est_pas_le_crc32_usuel)                                                                             \
    X(constantes_du_format_lues_dans_idf)                                                                              \
    X(etat_d_une_entree_est_sur_deux_bits)                                                                             \
    X(en_tete_de_l_outil_est_valide_meme_chiffre)                                                                      \
    X(une_entree_chiffree_n_est_pas_lisible)                                                                           \
    X(page_vierge_est_vide)                                                                                            \
    X(page_en_clair_de_l_outil_officiel)                                                                               \
    X(page_chiffree_de_l_outil_officiel)                                                                               \
    X(partition_de_trois_pages_comme_l_outil_la_produit)                                                               \
    X(un_secret_efface_reste_lisible_dans_la_flash)                                                                    \
    X(page_formatee_sans_entree_est_sans_donnee)                                                                       \
    X(clair_et_chiffre_ensemble_sont_melanges)                                                                         \
    X(une_seule_entree_abimee_dans_du_clair_rend_la_partition_melangee)                                                \
    X(page_etrangere_ou_a_demi_vierge_est_inconnue)                                                                    \
    X(une_page_inconnue_l_emporte_sur_tout)                                                                            \
    X(taille_nulle_ou_incomplete_n_est_jamais_vide)                                                                    \
    X(entree_de_span_nul_n_arrete_pas_la_lecture)                                                                      \
    X(entree_de_span_trois_cache_les_deux_suivantes)                                                                   \
    X(entree_effacee_de_span_nul_n_est_comptee_qu_une_fois)                                                            \
    X(mode_clair_ouvre_ce_qui_se_lit_sans_cle)                                                                         \
    X(mode_clair_refuse_une_partition_chiffree)                                                                        \
    X(partition_melangee_ou_etrangere_est_toujours_refusee)                                                            \
    X(mode_chiffre_sans_cle_n_ouvre_jamais)                                                                            \
    X(mode_chiffre_partition_vierge_s_ouvre)                                                                           \
    X(mode_chiffre_du_clair_attend_l_ordre_puis_migre)                                                                 \
    X(mode_chiffre_sans_donnee_purge_les_restes_sans_ordre)                                                            \
    X(mode_chiffre_illisible_exige_une_cle_essayee)                                                                    \
    X(invariants_sur_toutes_les_situations)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
