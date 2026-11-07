// Tests du noyau pur : migration de la NVS du clair vers le chiffré (nvs_securite.hpp : Instantane, migrer, reprendre).
//
// La carte est remplacée par une doublure en mémoire (`FauxNvs`) qui range les valeurs « en clair » ou « chiffrées », note chaque appel
// et sait tomber en panne à chaque étape. On vérifie l'ORDRE des appels (rien n'est purgé avant que tout soit lu), ce qui est gardé en
// cas d'échec (l'instantané, seule copie des valeurs), ce qui est effacé (l'instantané après réussite), et que « Fait » exige la
// relecture octet pour octet ET la preuve par lecture brute qu'il ne reste rien de lisible.
// Pas de static_assert ici (méthodes virtuelles) : ces tests ne sont vus qu'en LANÇANT le programme (voir lancer.py).
// Rien n'a tourné sur une carte : la vraie NVS, le vrai chiffrement et la vraie flash ne sont pas dans ce test.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "ecran/nvs_securite.hpp"
#include "mini.hpp"

namespace n = ecran::nvs_securite;
using n::Issue;
using n::Mode;

namespace {

struct Valeur {
    std::string espace;
    std::string cle;
    std::uint8_t type = 0;
    std::vector<std::uint8_t> octets;
    bool operator==(const Valeur &o) const {
        return espace == o.espace && cle == o.cle && type == o.type && octets == o.octets;
    }
};

Valeur texte(const char *espace, const char *cle, const char *valeur) {
    Valeur v;
    v.espace = espace;
    v.cle = cle;
    v.type = 0x21; // NVS_TYPE_STR
    v.octets.assign(valeur, valeur + std::strlen(valeur) + 1);
    return v;
}

Valeur entier(const char *espace, const char *cle, std::int32_t valeur) {
    Valeur v;
    v.espace = espace;
    v.cle = cle;
    v.type = 0x14; // NVS_TYPE_I32
    for (int i = 0; i < 4; ++i) {
        v.octets.push_back(static_cast<std::uint8_t>(static_cast<std::uint32_t>(valeur) >> (8 * i)));
    }
    return v;
}

Valeur blob(const char *espace, const char *cle, std::vector<std::uint8_t> octets) {
    Valeur v;
    v.espace = espace;
    v.cle = cle;
    v.type = 0x42; // NVS_TYPE_BLOB
    v.octets = std::move(octets);
    return v;
}

class FauxNvs final : public n::PlateformeNvs {
  public:
    std::vector<Valeur> clair;   // valeurs vivantes, lisibles sans clé
    std::vector<Valeur> chiffre; // valeurs vivantes, chiffrées
    int restes_clair = 0;        // entrées effacées dont le contenu est encore lisible dans la flash
    std::string journal;         // « lire_tout(clair);fermer;purger;… »

    // pannes et mensonges possibles, tous éteints au départ
    bool lecture_echoue = false;
    bool lecture_muette = false;   // la lecture réussit mais ne rend rien
    bool purge_echoue = false;     // la purge échoue sans rien effacer
    bool purge_mensongere = false; // la purge répond « oui » sans rien effacer
    int restes_apres_purge = 0;    // la purge réussit mais laisse des entrées effacées lisibles
    bool ouverture_echoue = false;
    int ecriture_echoue_au_rang = -1;
    bool validation_echoue = false;
    int relecture_alteree_au_rang = -1;
    bool relecture_tronquee = false;
    bool ecrit_en_clair = false;     // « chiffre » en vrai : range les valeurs en clair
    int restes_apres_validation = 0; // la validation laisse des entrées lisibles effacées

    bool lire_tout(Mode depuis, n::Instantane &inst) override {
        note(std::string("lire_tout(") + (depuis == Mode::Clair ? "clair" : "chiffre") + ")");
        if (lecture_echoue) {
            return false;
        }
        if (lecture_muette) {
            return true;
        }
        for (const Valeur &v : (depuis == Mode::Clair ? clair : chiffre)) {
            if (!inst.ajouter(v.espace.c_str(), v.cle.c_str(), v.type, v.octets.data(), v.octets.size())) {
                return false;
            }
        }
        return true;
    }
    bool purger() override {
        note("purger");
        if (purge_echoue) {
            return false;
        }
        if (purge_mensongere) {
            return true;
        }
        clair.clear();
        chiffre.clear();
        restes_clair = restes_apres_purge;
        return true;
    }
    bool bilan_brut(n::Bilan &b) override {
        note("bilan");
        b = n::Bilan{};
        const bool formatee = !clair.empty() || !chiffre.empty() || restes_clair > 0;
        b.pages_formatees = formatee ? 1 : 0;
        b.pages_vides = formatee ? 2 : 3;
        b.entrees_lisibles = static_cast<std::uint32_t>(clair.size());
        b.entrees_illisibles = static_cast<std::uint32_t>(chiffre.size());
        b.restes_lisibles = static_cast<std::uint32_t>(restes_clair);
        return true;
    }
    bool ouvrir(Mode mode) override {
        note(std::string("ouvrir(") + (mode == Mode::Clair ? "clair" : "chiffre") + ")");
        return !ouverture_echoue;
    }
    bool ecrire(const n::Entree &e, const std::uint8_t *valeur) override {
        note(std::string("ecrire(") + e.cle + ")");
        if (ecriture_echoue_au_rang == ecrites_) {
            return false;
        }
        ++ecrites_;
        Valeur v;
        v.espace = e.espace;
        v.cle = e.cle;
        v.type = e.type;
        v.octets.assign(valeur, valeur + e.taille);
        (ecrit_en_clair ? clair : chiffre).push_back(v);
        return true;
    }
    bool valider() override {
        note("valider");
        restes_clair += restes_apres_validation;
        return !validation_echoue;
    }
    bool relire(const n::Entree &e, std::uint8_t *tampon, std::size_t capacite, std::size_t &taille) override {
        note(std::string("relire(") + e.cle + ")");
        for (const Valeur &v : (ecrit_en_clair ? clair : chiffre)) {
            if (v.espace == e.espace && v.cle == e.cle) {
                taille = v.octets.size();
                if (relecture_tronquee && taille > 0) {
                    --taille;
                }
                if (taille > capacite) {
                    return false;
                }
                for (std::size_t i = 0; i < taille; ++i) {
                    tampon[i] = v.octets[i];
                }
                if (relecture_alteree_au_rang == relues_ && taille > 0) {
                    tampon[0] = static_cast<std::uint8_t>(tampon[0] ^ 0x01);
                }
                ++relues_;
                return true;
            }
        }
        return false;
    }
    void fermer() override { note("fermer"); }

  private:
    int ecrites_ = 0;
    int relues_ = 0;
    void note(const std::string &appel) {
        if (!journal.empty()) {
            journal += ';';
        }
        journal += appel;
    }
};

const std::vector<Valeur> &valeurs_d_essai() {
    static const std::vector<Valeur> v = {
        texte("wifi", "ssid", "ReseauEssai"),
        texte("wifi", "mdp", "essai-mot-de-passe-0001"),
        entier("services", "llm.preset", 2),
        blob("zone", "pos", {0x00, 0x11, 0xFF, 0x00, 0x7F}),
    };
    return v;
}

FauxNvs nvs_en_clair() {
    FauxNvs f;
    f.clair = valeurs_d_essai();
    return f;
}

bool toute_a_zero(const n::Instantane &inst) {
    for (std::size_t i = 0; i < n::OCTETS_MAX; ++i) {
        if (inst.octets[i] != 0) {
            return false;
        }
    }
    const auto *brut = reinterpret_cast<const unsigned char *>(inst.entrees);
    for (std::size_t i = 0; i < sizeof(inst.entrees); ++i) {
        if (brut[i] != 0) {
            return false;
        }
    }
    return inst.nombre == 0 && inst.utilises == 0;
}

bool contient(const std::string &journal, const char *appel) { return journal.find(appel) != std::string::npos; }

const char *const JOURNAL_NOMINAL = "lire_tout(clair);fermer;purger;bilan;ouvrir(chiffre);ecrire(ssid);ecrire(mdp);"
                                    "ecrire(llm.preset);ecrire(pos);valider;"
                                    "relire(ssid);relire(mdp);relire(llm.preset);relire(pos);fermer;bilan";

// ---- instantané -------------------------------------------------------------------------------------------------------------

int instantane_garde_et_rend_les_valeurs() {
    n::Instantane inst;
    const std::uint8_t a[] = {1, 2, 3};
    const std::uint8_t b[] = {0, 0xFF};
    VERIFIER(inst.ajouter("wifi", "ssid", 0x21, a, 3));
    VERIFIER(inst.ajouter("zone", "pos", 0x42, b, 2));
    VERIFIER(inst.nombre == 2 && inst.utilises == 5);
    VERIFIER(std::strcmp(inst.entrees[0].espace, "wifi") == 0 && std::strcmp(inst.entrees[0].cle, "ssid") == 0);
    VERIFIER(inst.entrees[0].type == 0x21 && inst.entrees[0].taille == 3 && inst.entrees[0].debut == 0);
    VERIFIER(inst.entrees[1].taille == 2 && inst.entrees[1].debut == 3);
    VERIFIER(std::memcmp(inst.valeur(inst.entrees[0]), a, 3) == 0);
    VERIFIER(std::memcmp(inst.valeur(inst.entrees[1]), b, 2) == 0);
    VERIFIER(inst.ajouter("zone", "vide", 0x42, nullptr, 0)); // une valeur vide est permise
    VERIFIER(inst.nombre == 3 && inst.utilises == 5);
    return 0;
}

int instantane_refuse_ce_qui_ne_tient_pas() {
    n::Instantane inst;
    const std::uint8_t un[] = {7};
    VERIFIER(inst.ajouter("123456789012345", "123456789012345", 1, un, 1)); // 15 caractères : la limite de NVS
    VERIFIER(inst.nombre == 1 && inst.utilises == 1);
    const std::size_t nombre = inst.nombre;
    const std::size_t utilises = inst.utilises;
    VERIFIER(!inst.ajouter("1234567890123456", "k", 1, un, 1)); // 16 caractères : trop long
    VERIFIER(!inst.ajouter("e", "1234567890123456", 1, un, 1));
    VERIFIER(!inst.ajouter("", "k", 1, un, 1)); // nom vide
    VERIFIER(!inst.ajouter("e", "", 1, un, 1));
    VERIFIER(!inst.ajouter(nullptr, "k", 1, un, 1));
    VERIFIER(!inst.ajouter("e", nullptr, 1, un, 1));
    VERIFIER(!inst.ajouter("e", "k", 1, nullptr, 1));             // taille sans données
    VERIFIER(inst.nombre == nombre && inst.utilises == utilises); // un refus ne change rien
    return 0;
}

int instantane_plein_refuse_la_valeur_suivante() {
    n::Instantane inst;
    const std::uint8_t un[] = {1};
    for (std::size_t i = 0; i < n::ENTREES_MAX; ++i) {
        VERIFIER(inst.ajouter("e", ("k" + std::to_string(i)).c_str(), 1, un, 1));
    }
    VERIFIER(inst.nombre == n::ENTREES_MAX);
    VERIFIER(!inst.ajouter("e", "de_trop", 1, un, 1));
    VERIFIER(inst.nombre == n::ENTREES_MAX && inst.utilises == n::ENTREES_MAX);

    n::Instantane gros;
    std::vector<std::uint8_t> octets(n::OCTETS_MAX, 0xAB);
    VERIFIER(!gros.ajouter("e", "k", 1, octets.data(), n::OCTETS_MAX + 1));
    VERIFIER(gros.nombre == 0 && gros.utilises == 0);
    VERIFIER(gros.ajouter("e", "k", 1, octets.data(), n::OCTETS_MAX)); // ajuste exactement
    VERIFIER(gros.utilises == n::OCTETS_MAX);
    VERIFIER(!gros.ajouter("e", "k2", 1, octets.data(), 1)); // plus un seul octet de place
    VERIFIER(gros.nombre == 1 && gros.utilises == n::OCTETS_MAX);
    VERIFIER(gros.ajouter("e", "k3", 1, nullptr, 0)); // mais une valeur vide tient encore
    return 0;
}

int instantane_efface_met_tout_a_zero() {
    n::Instantane inst;
    const std::uint8_t secret[] = {0x73, 0x65, 0x63, 0x72, 0x65, 0x74};
    VERIFIER(inst.ajouter("wifi", "mdp", 0x21, secret, sizeof(secret)));
    VERIFIER(!toute_a_zero(inst));
    inst.effacer();
    VERIFIER(toute_a_zero(inst));
    VERIFIER(inst.ajouter("wifi", "mdp", 0x21, secret, sizeof(secret))); // et il se réutilise
    VERIFIER(inst.nombre == 1 && inst.entrees[0].debut == 0);
    return 0;
}

// ---- migration --------------------------------------------------------------------------------------------------------------

int migration_nominale_dans_l_ordre() {
    FauxNvs f = nvs_en_clair();
    n::Instantane inst;
    VERIFIER(n::migrer(f, Mode::Clair, inst) == Issue::Fait);
    VERIFIER(f.journal == JOURNAL_NOMINAL);
    VERIFIER(f.clair.empty() && f.restes_clair == 0); // plus rien de lisible
    VERIFIER(f.chiffre == valeurs_d_essai());         // toutes les valeurs, octet pour octet, type compris
    VERIFIER(toute_a_zero(inst));                     // les secrets ne restent pas en mémoire
    return 0;
}

int les_valeurs_binaires_traversent_sans_alteration() {
    FauxNvs f = nvs_en_clair();
    n::Instantane inst;
    VERIFIER(n::migrer(f, Mode::Clair, inst) == Issue::Fait);
    // le blob contient 0x00, 0xFF et 0x7F ; le texte garde son zéro final
    VERIFIER(f.chiffre.size() == 4 && f.chiffre[3].octets == std::vector<std::uint8_t>({0x00, 0x11, 0xFF, 0x00, 0x7F}));
    VERIFIER(f.chiffre[1].octets.back() == 0 && f.chiffre[1].octets.size() == 24);
    return 0;
}

int lecture_en_echec_ne_touche_a_rien() {
    FauxNvs f = nvs_en_clair();
    f.lecture_echoue = true;
    n::Instantane inst;
    VERIFIER(n::migrer(f, Mode::Clair, inst) == Issue::EchecLecture);
    VERIFIER(!contient(f.journal, "purger") && !contient(f.journal, "ouvrir"));
    VERIFIER(f.clair == valeurs_d_essai() && f.chiffre.empty()); // la flash n'a pas bougé
    VERIFIER(!n::a_reprendre(Issue::EchecLecture) && toute_a_zero(inst));
    return 0;
}

int trop_de_valeurs_pour_l_instantane_ne_purge_pas() {
    FauxNvs f;
    for (std::size_t i = 0; i <= n::ENTREES_MAX; ++i) { // une de plus que la place
        f.clair.push_back(entier("e", ("k" + std::to_string(i)).c_str(), static_cast<std::int32_t>(i)));
    }
    n::Instantane inst;
    VERIFIER(n::migrer(f, Mode::Clair, inst) == Issue::EchecLecture);
    VERIFIER(!contient(f.journal, "purger"));
    VERIFIER(f.clair.size() == n::ENTREES_MAX + 1 && toute_a_zero(inst));
    return 0;
}

int rien_lu_d_une_partition_claire_ne_purge_pas() {
    FauxNvs f = nvs_en_clair();
    f.lecture_muette = true;
    n::Instantane inst;
    VERIFIER(n::migrer(f, Mode::Clair, inst) == Issue::InstantaneVide);
    VERIFIER(!contient(f.journal, "purger"));
    VERIFIER(f.clair == valeurs_d_essai() && toute_a_zero(inst) && !n::a_reprendre(Issue::InstantaneVide));
    return 0;
}

int purge_qui_echoue_garde_l_instantane_et_se_reprend() {
    FauxNvs f = nvs_en_clair();
    f.purge_echoue = true;
    n::Instantane inst;
    VERIFIER(n::migrer(f, Mode::Clair, inst) == Issue::EchecPurge);
    VERIFIER(n::a_reprendre(Issue::EchecPurge));
    VERIFIER(!contient(f.journal, "ouvrir") && !contient(f.journal, "ecrire"));
    VERIFIER(inst.nombre == 4 && !toute_a_zero(inst)); // les valeurs ne sont encore que là et dans la flash
    VERIFIER(std::strcmp(reinterpret_cast<const char *>(inst.valeur(inst.entrees[1])), "essai-mot-de-passe-0001") == 0);
    f.purge_echoue = false; // la reprise se fait avec le même instantané
    VERIFIER(n::reprendre(f, inst) == Issue::Fait);
    VERIFIER(f.chiffre == valeurs_d_essai() && f.clair.empty() && toute_a_zero(inst));
    return 0;
}

int purge_non_prouvee_par_la_lecture_brute_est_un_echec() {
    FauxNvs f = nvs_en_clair();
    f.purge_mensongere = true; // « effacé » mais la flash porte toujours les valeurs en clair
    n::Instantane inst;
    VERIFIER(n::migrer(f, Mode::Clair, inst) == Issue::EchecPurge);
    VERIFIER(!contient(f.journal, "ouvrir")); // on n'écrit pas par-dessus une partition qu'on n'a pas vue vide
    VERIFIER(inst.nombre == 4);
    return 0;
}

int purge_qui_laisse_des_restes_lisibles_est_un_echec() {
    FauxNvs f = nvs_en_clair();
    f.restes_apres_purge = 2;
    n::Instantane inst;
    VERIFIER(n::migrer(f, Mode::Clair, inst) == Issue::EchecPurge);
    VERIFIER(!contient(f.journal, "ouvrir") && inst.nombre == 4);
    return 0;
}

int ouverture_chiffree_qui_echoue_garde_l_instantane() {
    FauxNvs f = nvs_en_clair();
    f.ouverture_echoue = true;
    n::Instantane inst;
    VERIFIER(n::migrer(f, Mode::Clair, inst) == Issue::EchecOuverture);
    VERIFIER(n::a_reprendre(Issue::EchecOuverture));
    VERIFIER(!contient(f.journal, "ecrire") && inst.nombre == 4);
    f.ouverture_echoue = false;
    VERIFIER(n::reprendre(f, inst) == Issue::Fait && f.chiffre == valeurs_d_essai());
    return 0;
}

int ecriture_qui_echoue_au_milieu_se_reprend_sans_doublon() {
    FauxNvs f = nvs_en_clair();
    f.ecriture_echoue_au_rang = 2;
    n::Instantane inst;
    VERIFIER(n::migrer(f, Mode::Clair, inst) == Issue::EchecEcriture);
    VERIFIER(n::a_reprendre(Issue::EchecEcriture));
    VERIFIER(f.journal.size() >= 20 && f.journal.substr(f.journal.size() - 25) == "ecrire(llm.preset);fermer");
    VERIFIER(!contient(f.journal, "valider")); // pas de validation d'une écriture incomplète
    VERIFIER(inst.nombre == 4 && f.chiffre.size() == 2);
    f.ecriture_echoue_au_rang = -1;
    VERIFIER(n::reprendre(f, inst) == Issue::Fait);
    VERIFIER(f.chiffre == valeurs_d_essai()); // exactement quatre valeurs : la reprise a purgé les deux premières
    return 0;
}

int validation_qui_echoue_est_un_echec_d_ecriture() {
    FauxNvs f = nvs_en_clair();
    f.validation_echoue = true;
    n::Instantane inst;
    VERIFIER(n::migrer(f, Mode::Clair, inst) == Issue::EchecEcriture);
    VERIFIER(!contient(f.journal, "relire") && f.journal.substr(f.journal.size() - 14) == "valider;fermer");
    VERIFIER(inst.nombre == 4);
    return 0;
}

int valeur_relue_differente_est_detectee() {
    for (int rang = 0; rang < 4;
         ++rang) { // la première, les deux du milieu et la dernière valeur : toutes sont comparées
        FauxNvs f = nvs_en_clair();
        f.relecture_alteree_au_rang = rang; // un seul octet de cette valeur change
        n::Instantane inst;
        VERIFIER(n::migrer(f, Mode::Clair, inst) == Issue::EchecVerification);
        VERIFIER(n::a_reprendre(Issue::EchecVerification) && inst.nombre == 4);
        VERIFIER(f.journal.substr(f.journal.size() - 6) ==
                 "fermer"); // la partition est refermée avant de rendre la main
    }
    return 0;
}

int valeur_relue_plus_courte_est_detectee() {
    FauxNvs f = nvs_en_clair();
    f.relecture_tronquee = true;
    n::Instantane inst;
    VERIFIER(n::migrer(f, Mode::Clair, inst) == Issue::EchecVerification);
    VERIFIER(inst.nombre == 4);
    return 0;
}

int ecriture_en_clair_est_vue_par_la_lecture_brute() {
    FauxNvs f = nvs_en_clair();
    f.ecrit_en_clair =
        true; // « chiffré » en vrai : les valeurs se relisent très bien, mais restent lisibles dans la flash
    n::Instantane inst;
    VERIFIER(n::migrer(f, Mode::Clair, inst) == Issue::RestesEnClair);
    VERIFIER(n::a_reprendre(Issue::RestesEnClair) && inst.nombre == 4);
    return 0;
}

int un_reste_lisible_apres_l_ecriture_est_vu_par_la_lecture_brute() {
    FauxNvs f = nvs_en_clair();
    f.restes_apres_validation = 1;
    n::Instantane inst;
    VERIFIER(n::migrer(f, Mode::Clair, inst) == Issue::RestesEnClair);
    VERIFIER(inst.nombre == 4);
    return 0;
}

int une_nvs_chiffree_a_restes_en_clair_se_recopie_aussi() {
    FauxNvs f;
    f.chiffre = valeurs_d_essai();
    f.restes_clair = 3; // d'anciens secrets effacés mais lisibles
    n::Instantane inst;
    VERIFIER(n::migrer(f, Mode::Chiffre, inst) == Issue::Fait);
    VERIFIER(f.journal.compare(0, 18, "lire_tout(chiffre)") == 0);
    VERIFIER(f.chiffre == valeurs_d_essai() && f.restes_clair == 0 && f.clair.empty());
    return 0;
}

int l_instantane_d_avant_n_est_pas_repris() {
    FauxNvs f = nvs_en_clair();
    f.purge_echoue = true; // on s'arrête avec l'instantané plein
    n::Instantane inst;
    const std::uint8_t reste[] = {9, 9};
    VERIFIER(inst.ajouter("vieux", "reste", 1, reste, 2)); // une valeur laissée par un essai précédent
    VERIFIER(n::migrer(f, Mode::Clair, inst) == Issue::EchecPurge);
    VERIFIER(inst.nombre == 4 &&
             inst.utilises == 12 + 24 + 4 + 5); // les quatre valeurs d'essai seulement, pas celle d'avant
    return 0;
}

int a_reprendre_dit_vrai_seulement_quand_les_valeurs_ne_sont_qu_en_memoire() {
    VERIFIER(!n::a_reprendre(Issue::Fait));
    VERIFIER(!n::a_reprendre(Issue::EchecLecture));
    VERIFIER(!n::a_reprendre(Issue::InstantaneVide));
    VERIFIER(n::a_reprendre(Issue::EchecPurge));
    VERIFIER(n::a_reprendre(Issue::EchecOuverture));
    VERIFIER(n::a_reprendre(Issue::EchecEcriture));
    VERIFIER(n::a_reprendre(Issue::EchecVerification));
    VERIFIER(n::a_reprendre(Issue::RestesEnClair));
    return 0;
}

#define LISTE(X)                                                                                                       \
    X(instantane_garde_et_rend_les_valeurs)                                                                            \
    X(instantane_refuse_ce_qui_ne_tient_pas)                                                                           \
    X(instantane_plein_refuse_la_valeur_suivante)                                                                      \
    X(instantane_efface_met_tout_a_zero)                                                                               \
    X(migration_nominale_dans_l_ordre)                                                                                 \
    X(les_valeurs_binaires_traversent_sans_alteration)                                                                 \
    X(lecture_en_echec_ne_touche_a_rien)                                                                               \
    X(trop_de_valeurs_pour_l_instantane_ne_purge_pas)                                                                  \
    X(rien_lu_d_une_partition_claire_ne_purge_pas)                                                                     \
    X(purge_qui_echoue_garde_l_instantane_et_se_reprend)                                                               \
    X(purge_non_prouvee_par_la_lecture_brute_est_un_echec)                                                             \
    X(purge_qui_laisse_des_restes_lisibles_est_un_echec)                                                               \
    X(ouverture_chiffree_qui_echoue_garde_l_instantane)                                                                \
    X(ecriture_qui_echoue_au_milieu_se_reprend_sans_doublon)                                                           \
    X(validation_qui_echoue_est_un_echec_d_ecriture)                                                                   \
    X(valeur_relue_differente_est_detectee)                                                                            \
    X(valeur_relue_plus_courte_est_detectee)                                                                           \
    X(ecriture_en_clair_est_vue_par_la_lecture_brute)                                                                  \
    X(un_reste_lisible_apres_l_ecriture_est_vu_par_la_lecture_brute)                                                   \
    X(une_nvs_chiffree_a_restes_en_clair_se_recopie_aussi)                                                             \
    X(l_instantane_d_avant_n_est_pas_repris)                                                                           \
    X(a_reprendre_dit_vrai_seulement_quand_les_valeurs_ne_sont_qu_en_memoire)

} // namespace

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
