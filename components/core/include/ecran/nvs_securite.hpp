// Noyau pur : SÉCURITÉ DE LA NVS — savoir ce que contient la partition AVANT de l'ouvrir, décider quoi en faire, et migrer du clair vers
// le chiffré sans rien perdre ni rien laisser en clair (docs/recherche/07-chiffrement-nvs.md ; ADR-0003, point 5).
//
// Pourquoi ce fichier : ouvrir la NVS dans le mauvais mode ne renvoie pas d'erreur, elle EFFACE. Une partition chiffrée ouverte sans clé,
// une partition en clair ouverte avec des clés, ou une mauvaise clé : chaque entrée dont la somme de contrôle ne tombe pas juste est
// effacée sur place, sans rien dire (nvs_page.cpp, Page::load, lu dans ESP-IDF 5.5.5 ; jamais constaté sur une carte). Et un
// `nvs_flash_init()` compilé avec le chiffrement par clé HMAC BRÛLE un bloc d'eFuse si la clé n'existe pas encore, sans rien demander
// (nvs_sec_provider.c, lu ; non constaté). On lit donc la partition EN BRUT (esp_partition_read, aucun effet de bord), on la classe, on
// décide, et seulement ensuite on l'ouvre.
//
// Trois morceaux, tous évalués sur le PC (test/host/test_nvs_securite.cpp) :
//   1. `bilan_page` / `bilan_partition` / `classer` : lisent le FORMAT des pages NVS (en-tête, table d'états, entrées de 32 octets et leur
//      somme de contrôle). Les entrées chiffrées sont illisibles SANS clé : leur somme de contrôle est fausse, c'est ce qui les distingue
//      des entrées en clair. Un secret EFFACÉ en clair reste lisible dans la flash (seul son état change) : `restes_lisibles` les compte.
//   2. `decider` : la table « contenu × mode demandé × clé × ordre » ; elle interdit d'ouvrir dans un mode qui effacerait, de laisser la
//      NVS créer la clé, et de migrer sans l'ordre d'Axel.
//   3. `migrer` / `reprendre` : copie tout en mémoire vive, efface PHYSIQUEMENT la partition, la prouve vide, l'ouvre chiffrée, réécrit,
//      relit chaque valeur octet pour octet et vérifie par une lecture brute qu'il ne reste rien de lisible.
//
// Limites écrites d'avance : (a) ce fichier ne touche à aucune flash ni à aucun eFuse : tout passe par `PlateformeNvs`, à brancher dans
// main/ ou dans un BSP (PAS fait à ce jour) ; (b) entre la purge et la fin de l'écriture, les valeurs n'existent qu'en mémoire vive : une
// coupure de courant à ce moment les perd (le Wi-Fi et les clés se ressaisissent, rien d'autre n'est perdu) ; (c) une entrée chiffrée
// dont la somme de contrôle tomberait juste par hasard (une chance sur 2^32 par entrée) serait prise pour du clair : la partition serait
// classée « mélangée » et refusée, jamais effacée ; (d) la clé n'est pas essayée ici : `cle_essayee` doit venir d'un déchiffrement réel
// d'une entrée (non codé). C++17 standard, aucun include ESP-IDF.
#pragma once

#include <cstddef>
#include <cstdint>

namespace ecran::nvs_securite {

// ---- format d'une page NVS (nvs_constants.h, nvs_page.hpp, nvs_page.cpp, ESP-IDF 5.5.5) --------------------------------------
inline constexpr std::size_t TAILLE_PAGE = 4096;
inline constexpr std::size_t TAILLE_ENTREE = 32;
inline constexpr std::size_t ENTREES_PAR_PAGE = 126;
inline constexpr std::size_t DECALAGE_TABLE = 32;   // table d'états : 32 octets, 2 bits par entrée
inline constexpr std::size_t DECALAGE_ENTREES = 64; // 32 octets d'en-tête de page + 32 octets de table
// État d'une page (premier mot de l'en-tête) : on ne fait que descendre, un bit à la fois, de 0xFFFFFFFF.
inline constexpr std::uint32_t PAGE_ACTIVE = 0xFFFFFFFEu;
inline constexpr std::uint32_t PAGE_PLEINE = 0xFFFFFFFCu;
inline constexpr std::uint32_t PAGE_EN_LIBERATION = 0xFFFFFFF8u;
inline constexpr std::uint32_t PAGE_ABIMEE = 0xFFFFFFF0u;
// État d'une entrée (2 bits).
inline constexpr unsigned ENTREE_EFFACEE =
    0; // écrite puis effacée : seul ce bit change, les 32 octets restent dans la flash
inline constexpr unsigned ENTREE_ECRITE = 2;

namespace detail {

constexpr std::uint32_t lire_u32(const std::uint8_t *p) noexcept {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

// CRC-32 réfléchi, polynôme 0xEDB88320, tel que le calcule NVS : esp_rom_crc32_le(0xFFFFFFFF, …), c'est-à-dire état de départ 0 et
// inversion finale (équivalent de zlib.crc32(octets, 0xFFFFFFFF), pas du CRC-32 usuel).
constexpr std::uint32_t crc_octet(std::uint32_t crc, std::uint8_t octet) noexcept {
    crc ^= octet;
    for (int i = 0; i < 8; ++i) {
        crc = (crc >> 1) ^ ((crc & 1u) != 0 ? 0xEDB88320u : 0u);
    }
    return crc;
}

constexpr bool page_toute_a_ff(const std::uint8_t *page) noexcept {
    for (std::size_t i = 0; i < TAILLE_PAGE; ++i) {
        if (page[i] != 0xFF) {
            return false;
        }
    }
    return true;
}

} // namespace detail

// Somme de contrôle NVS d'une suite d'octets (voir plus haut pour la graine).
constexpr std::uint32_t crc_nvs(const std::uint8_t *octets, std::size_t nombre) noexcept {
    std::uint32_t crc = 0;
    for (std::size_t i = 0; i < nombre; ++i) {
        crc = detail::crc_octet(crc, octets[i]);
    }
    return ~crc;
}

// En-tête d'une page formatée : état connu et somme de contrôle (octets 4 à 27) juste. L'en-tête n'est JAMAIS chiffré : une partition
// chiffrée a des en-têtes valides, ce sont ses entrées qui sont illisibles.
constexpr bool entete_valide(const std::uint8_t *page) noexcept {
    const std::uint32_t etat = detail::lire_u32(page);
    if (etat != PAGE_ACTIVE && etat != PAGE_PLEINE && etat != PAGE_EN_LIBERATION && etat != PAGE_ABIMEE) {
        return false;
    }
    return crc_nvs(page + 4, 24) == detail::lire_u32(page + 28);
}

// Une entrée de 32 octets est lisible en clair quand sa somme de contrôle (octets 0 à 3 puis 8 à 31) est celle qu'elle porte (octets 4 à 7).
constexpr bool entree_lisible(const std::uint8_t *entree) noexcept {
    std::uint32_t crc = 0;
    for (std::size_t i = 0; i < 4; ++i) {
        crc = detail::crc_octet(crc, entree[i]);
    }
    for (std::size_t i = 8; i < TAILLE_ENTREE; ++i) {
        crc = detail::crc_octet(crc, entree[i]);
    }
    return ~crc == detail::lire_u32(entree + 4);
}

constexpr unsigned etat_entree(const std::uint8_t *page, std::size_t rang) noexcept {
    return (static_cast<unsigned>(page[DECALAGE_TABLE + rang / 4]) >> (2 * (rang % 4))) & 3u;
}

// ---- ce que contient une partition -----------------------------------------------------------------------------------------

struct Bilan {
    std::uint32_t pages_vides = 0;        // page entièrement à 0xFF
    std::uint32_t pages_formatees = 0;    // en-tête valide
    std::uint32_t pages_inconnues = 0;    // ni vide ni en-tête valide : pas une NVS, ou une page abîmée
    std::uint32_t entrees_lisibles = 0;   // écrites, somme de contrôle juste : EN CLAIR
    std::uint32_t entrees_illisibles = 0; // écrites, somme de contrôle fausse : chiffrées (clé inconnue) ou abîmées
    std::uint32_t restes_lisibles = 0;    // EFFACÉES mais encore lisibles en clair dans la flash
};

// `page` : exactement TAILLE_PAGE octets. Une entrée écrite en occupe `span` (octet 2) : la valeur d'un texte ou d'un blob suit son
// entrée d'en-tête et n'est pas une entrée ; on la saute quand l'en-tête est lisible, sinon on avance d'une entrée.
constexpr Bilan bilan_page(const std::uint8_t *page) noexcept {
    Bilan b;
    if (detail::page_toute_a_ff(page)) {
        b.pages_vides = 1;
        return b;
    }
    if (!entete_valide(page)) {
        b.pages_inconnues = 1;
        return b;
    }
    b.pages_formatees = 1;
    std::size_t rang = 0;
    while (rang < ENTREES_PAR_PAGE) {
        const unsigned etat = etat_entree(page, rang);
        if (etat != ENTREE_ECRITE && etat != ENTREE_EFFACEE) {
            ++rang; // vide, ou état incohérent : rien à compter
            continue;
        }
        const std::uint8_t *entree = page + DECALAGE_ENTREES + rang * TAILLE_ENTREE;
        const bool lisible = entree_lisible(entree);
        if (lisible) {
            if (etat == ENTREE_ECRITE) {
                ++b.entrees_lisibles;
            } else {
                ++b.restes_lisibles;
            }
        } else if (etat == ENTREE_ECRITE) {
            ++b.entrees_illisibles;
        }
        rang += (lisible && entree[2] > 0) ? entree[2] : 1;
    }
    return b;
}

constexpr void ajouter(Bilan &total, const Bilan &page) noexcept {
    total.pages_vides += page.pages_vides;
    total.pages_formatees += page.pages_formatees;
    total.pages_inconnues += page.pages_inconnues;
    total.entrees_lisibles += page.entrees_lisibles;
    total.entrees_illisibles += page.entrees_illisibles;
    total.restes_lisibles += page.restes_lisibles;
}

// Toute la partition d'un coup (tests, outils). Sur la carte, `bilan_page` page par page : 4 Ko de mémoire vive suffisent.
// Une taille nulle ou qui n'est pas un nombre de pages entières compte pour une page inconnue : jamais « vide » par défaut.
constexpr Bilan bilan_partition(const std::uint8_t *octets, std::size_t taille) noexcept {
    Bilan total;
    if (octets == nullptr || taille == 0 || taille % TAILLE_PAGE != 0) {
        total.pages_inconnues = 1;
        return total;
    }
    for (std::size_t p = 0; p < taille / TAILLE_PAGE; ++p) {
        ajouter(total, bilan_page(octets + p * TAILLE_PAGE));
    }
    return total;
}

enum class Contenu : std::uint8_t {
    Vide,       // tout à 0xFF : rien n'a jamais été écrit (ou tout a été purgé)
    SansDonnee, // formatée, aucune entrée écrite (il peut rester des entrées effacées : voir `restes_lisibles`)
    Claire,     // des entrées lisibles sans clé, aucune illisible
    Illisible,  // des entrées écrites que seule une clé lit (ou abîmées), aucune lisible
    Melangee,   // les deux : à ne pas ouvrir, quel que soit le mode
    Inconnue    // une page qui n'est ni vide ni une page NVS
};

constexpr Contenu classer(const Bilan &b) noexcept {
    if (b.pages_inconnues > 0) {
        return Contenu::Inconnue;
    }
    if (b.entrees_lisibles > 0 && b.entrees_illisibles > 0) {
        return Contenu::Melangee;
    }
    if (b.entrees_illisibles > 0) {
        return Contenu::Illisible;
    }
    if (b.entrees_lisibles > 0) {
        return Contenu::Claire;
    }
    return b.pages_formatees > 0 ? Contenu::SansDonnee : Contenu::Vide;
}

// ---- que faire avant d'ouvrir ----------------------------------------------------------------------------------------------

enum class Mode : std::uint8_t { Clair, Chiffre };

struct Situation {
    Contenu contenu = Contenu::Vide;
    // `restes_lisibles > 0` : des secrets effacés sont encore lisibles dans la flash.
    bool restes = false;
    // Le mode dans lequel le micrologiciel s'apprête à ouvrir (réglage de compilation).
    Mode mode = Mode::Clair;
    // La clé de chiffrement EXISTE déjà (eFuse ou partition de clés) : l'ouvrir ne la créerait pas.
    bool cle_existe = false;
    // La clé a déchiffré une vraie entrée de la partition (n'a de sens que pour `Illisible`).
    bool cle_essayee = false;
    // Axel a demandé de chiffrer : ordre explicite, lu sur l'écran ou dans un réglage d'installation.
    bool ordre = false;
};

enum class Decision : std::uint8_t {
    // Ouvrir dans le mode demandé : rien ne sera effacé, aucune clé créée.
    Ouvrir,
    // `migrer` : depuis le clair, ou depuis le chiffré pour purger des restes en clair.
    Migrer,
    // Aucune donnée vivante, seulement des restes : effacer physiquement la partition, puis recommencer.
    Purger,
    // Mode chiffré, clé absente : ouvrir la créerait (eFuse brûlé) ; seul Axel la crée, à la main : ne rien ouvrir.
    AttendreCle,
    // Des données en clair à chiffrer, mais personne n'a donné l'ordre : ne rien toucher.
    AttendreOrdre,
    // Ouvrir effacerait ou serait faux (mode discordant, clé non vérifiée, partition mélangée ou étrangère) : ne rien faire.
    Refuser
};

constexpr Decision decider(const Situation &s) noexcept {
    if (s.contenu == Contenu::Melangee || s.contenu == Contenu::Inconnue) {
        return Decision::Refuser;
    }
    if (s.mode == Mode::Clair) {
        // Ouvrir en clair une partition chiffrée effacerait toutes ses entrées : on s'arrête.
        return s.contenu == Contenu::Illisible ? Decision::Refuser : Decision::Ouvrir;
    }
    if (!s.cle_existe) {
        return Decision::AttendreCle;
    }
    switch (s.contenu) {
    case Contenu::Vide:
        return Decision::Ouvrir;
    case Contenu::SansDonnee:
        // Rien de vivant : la purge ne perd rien, donc pas besoin d'ordre.
        return s.restes ? Decision::Purger : Decision::Ouvrir;
    case Contenu::Claire:
        return s.ordre ? Decision::Migrer : Decision::AttendreOrdre;
    case Contenu::Illisible:
        if (!s.cle_essayee) {
            return Decision::Refuser; // une mauvaise clé effacerait tout en silence
        }
        return (s.restes && s.ordre) ? Decision::Migrer : Decision::Ouvrir;
    default:
        return Decision::Refuser;
    }
}

// ---- migration -------------------------------------------------------------------------------------------------------------

inline constexpr std::size_t NOM_MAX = 16;     // espace ou clé NVS : 15 caractères + zéro
inline constexpr std::size_t ENTREES_MAX = 48; // valeurs gardées en mémoire vive pendant la migration
// Total des octets de ces valeurs (texte avec son zéro final, blob). Pire cas LU dans le code (pas sur une carte) : trois services
// de ~620 octets (adresse 200, modèle 80, voix 80, clé 256), Wi-Fi ~100, cache météo 857 : ~3 Ko. 4 Ko laissent de la marge ; si la
// NVS réelle dépasse, `lire_tout` répond faux et RIEN n'est purgé (échec sûr, mais la migration ne se fait pas).
inline constexpr std::size_t OCTETS_MAX = 4096;

struct Entree {
    char espace[NOM_MAX] = {};
    char cle[NOM_MAX] = {};
    // nvs_type_t d'ESP-IDF (U8 0x01, I32 0x14, STR 0x21, BLOB 0x42…), rendu tel quel : le noyau ne l'interprète pas.
    std::uint8_t type = 0;
    std::uint16_t taille = 0; // octets de la valeur (zéro final d'un texte compris)
    std::uint16_t debut = 0;  // position dans `Instantane::octets`
};

// Copie en mémoire vive de TOUTES les valeurs. Elle contient des secrets : à effacer (`effacer`) dès qu'elle ne sert plus.
struct Instantane {
    Entree entrees[ENTREES_MAX];
    std::uint8_t octets[OCTETS_MAX] = {};
    std::size_t nombre = 0;
    std::size_t utilises = 0;

    // Faux (rien n'est ajouté) si un nom est trop long, si la valeur n'entre pas ou si l'une des deux limites est atteinte.
    bool ajouter(const char *espace, const char *cle, std::uint8_t type, const std::uint8_t *valeur,
                 std::size_t taille) {
        if (espace == nullptr || cle == nullptr || (valeur == nullptr && taille > 0) || nombre >= ENTREES_MAX ||
            taille > OCTETS_MAX - utilises) {
            return false;
        }
        Entree e;
        if (!copier_nom(e.espace, espace) || !copier_nom(e.cle, cle)) {
            return false;
        }
        e.type = type;
        e.taille = static_cast<std::uint16_t>(taille);
        e.debut = static_cast<std::uint16_t>(utilises);
        for (std::size_t i = 0; i < taille; ++i) {
            octets[utilises + i] = valeur[i];
        }
        utilises += taille;
        entrees[nombre++] = e;
        return true;
    }

    const std::uint8_t *valeur(const Entree &e) const { return octets + e.debut; }

    // Écrit des zéros PAR un pointeur volatile : le compilateur ne peut pas supprimer l'écriture parce que « personne ne relit ».
    void effacer() {
        volatile std::uint8_t *o = octets;
        for (std::size_t i = 0; i < OCTETS_MAX; ++i) {
            o[i] = 0;
        }
        volatile char *n = reinterpret_cast<volatile char *>(entrees);
        for (std::size_t i = 0; i < sizeof(entrees); ++i) {
            n[i] = 0;
        }
        nombre = 0;
        utilises = 0;
    }

  private:
    static bool copier_nom(char (&vers)[NOM_MAX], const char *depuis) {
        std::size_t i = 0;
        while (depuis[i] != '\0') {
            if (i + 1 >= NOM_MAX) {
                return false;
            }
            vers[i] = depuis[i];
            ++i;
        }
        vers[i] = '\0';
        return i > 0;
    }
};

// Ce que la migration demande à la carte (main/ ou un BSP). Rien ici n'est appelé hors de `migrer` et `reprendre`.
class PlateformeNvs {
  public:
    virtual ~PlateformeNvs() = default;

    // Ouvre la partition dans le mode où elle est ACTUELLEMENT et lit TOUTES les valeurs dans `inst`. Faux : la lecture est
    // incomplète (erreur, plus de place dans `inst`) ; la flash n'a pas bougé.
    virtual bool lire_tout(Mode depuis, Instantane &inst) = 0;
    // Efface PHYSIQUEMENT tous les secteurs de la partition (pas nvs_erase_all, qui ne change que des états). Ferme d'abord la NVS.
    virtual bool purger() = 0;
    // Lecture brute de la partition, sans l'ouvrir : le bilan de toutes ses pages.
    virtual bool bilan_brut(Bilan &bilan) = 0;
    virtual bool ouvrir(Mode mode) = 0;
    virtual bool ecrire(const Entree &entree, const std::uint8_t *valeur) = 0;
    virtual bool valider() = 0; // nvs_commit
    // Relit la valeur dans la NVS ouverte. Faux si elle est absente.
    virtual bool relire(const Entree &entree, std::uint8_t *tampon, std::size_t capacite, std::size_t &taille) = 0;
    virtual void fermer() = 0;
};

enum class Issue : std::uint8_t {
    // Tout est chiffré, relu à l'identique, rien de lisible ne reste ; l'instantané est effacé.
    Fait,
    // Lecture incomplète : RIEN n'a bougé, l'instantané est effacé.
    EchecLecture,
    // La lecture n'a rien rendu : on ne purge pas sur du vide ; RIEN n'a bougé, l'instantané est effacé.
    InstantaneVide,
    // La purge a échoué ou la lecture brute ne prouve pas la partition vide ; l'instantané est GARDÉ.
    EchecPurge,
    // La partition est vide mais ne s'ouvre pas en chiffré ; l'instantané est GARDÉ.
    EchecOuverture,
    // Une écriture ou la validation a échoué ; l'instantané est GARDÉ.
    EchecEcriture,
    // Une valeur relue diffère de l'instantané ; l'instantané est GARDÉ.
    EchecVerification,
    // Écrit et relu, mais la lecture brute voit du lisible ou rien de chiffré ; l'instantané est GARDÉ.
    RestesEnClair
};

// Vrai : les valeurs ne sont plus (toutes) dans la flash, l'instantané est leur seule copie. Il faut rappeler `reprendre` avec lui, ou
// demander à la personne de ressaisir ses réglages, et l'effacer ensuite.
constexpr bool a_reprendre(Issue i) noexcept {
    return i == Issue::EchecPurge || i == Issue::EchecOuverture || i == Issue::EchecEcriture ||
           i == Issue::EchecVerification || i == Issue::RestesEnClair;
}

namespace detail {

// Relit chaque valeur dans la NVS ouverte et la compare à l'instantané, octet pour octet. Le tampon de relecture porte des secrets :
// il est effacé avant de sortir (écriture volatile, que le compilateur ne supprime pas ; cet effacement n'est PAS couvert par un test).
inline bool relues_identiques(PlateformeNvs &p, const Instantane &inst) {
    std::uint8_t relu[OCTETS_MAX];
    bool identique = true;
    for (std::size_t i = 0; i < inst.nombre && identique; ++i) {
        const Entree &e = inst.entrees[i];
        std::size_t taille = 0;
        if (!p.relire(e, relu, sizeof(relu), taille) || taille != e.taille) {
            identique = false;
            break;
        }
        const std::uint8_t *attendu = inst.valeur(e);
        for (std::size_t k = 0; k < taille; ++k) {
            if (relu[k] != attendu[k]) {
                identique = false;
                break;
            }
        }
    }
    volatile std::uint8_t *v = relu;
    for (std::size_t k = 0; k < OCTETS_MAX; ++k) {
        v[k] = 0;
    }
    return identique;
}

} // namespace detail

// Du point où l'instantané est complet : purge, preuve du vide, ouverture chiffrée, écriture, relecture, preuve qu'il ne reste rien de
// lisible. Peut être rappelée telle quelle après un échec : la purge repart de zéro, il n'y a jamais de doublon.
inline Issue reprendre(PlateformeNvs &p, Instantane &inst) {
    if (!p.purger()) {
        return Issue::EchecPurge;
    }
    Bilan vide;
    if (!p.bilan_brut(vide) || classer(vide) != Contenu::Vide) {
        return Issue::EchecPurge;
    }
    if (!p.ouvrir(Mode::Chiffre)) {
        return Issue::EchecOuverture;
    }
    for (std::size_t i = 0; i < inst.nombre; ++i) {
        if (!p.ecrire(inst.entrees[i], inst.valeur(inst.entrees[i]))) {
            p.fermer();
            return Issue::EchecEcriture;
        }
    }
    if (!p.valider()) {
        p.fermer();
        return Issue::EchecEcriture;
    }
    const bool identique = detail::relues_identiques(p, inst);
    p.fermer();
    if (!identique) {
        return Issue::EchecVerification;
    }
    Bilan apres;
    if (!p.bilan_brut(apres) || classer(apres) != Contenu::Illisible || apres.restes_lisibles > 0) {
        return Issue::RestesEnClair;
    }
    inst.effacer();
    return Issue::Fait;
}

// Chiffre une NVS existante. `depuis` : le mode où elle est aujourd'hui (Clair, ou Chiffre pour purger les restes en clair d'une NVS
// déjà chiffrée). Appelée seulement quand `decider` a répondu `Migrer`. `inst` est à nous : effacé au début, effacé à la fin sauf si
// `a_reprendre(issue)`.
inline Issue migrer(PlateformeNvs &p, Mode depuis, Instantane &inst) {
    inst.effacer();
    const bool lu = p.lire_tout(depuis, inst);
    p.fermer();
    if (!lu) {
        inst.effacer();
        return Issue::EchecLecture;
    }
    if (inst.nombre == 0) {
        inst.effacer();
        return Issue::InstantaneVide;
    }
    return reprendre(p, inst);
}

} // namespace ecran::nvs_securite
