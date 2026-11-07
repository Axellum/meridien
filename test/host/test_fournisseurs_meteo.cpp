// Tests du noyau pur « sources météo » (fournisseurs_meteo.hpp) : la table des modèles d'Open-Meteo, la liste proposée pour un pays
// et ce que l'adresse demande.
//
// Le point dur : une source n'est proposée que si le LECTEUR du firmware (`analyser_riche`) garde sa réponse. Les 29 réponses de
// donnees/openmeteo_modele_*.json sont de VRAIES réponses d'Open-Meteo du 07/11/2026 à la requête complète de l'écran avec
// `&models=…` (test/host/capturer_modeles_meteo.py). Pour chacune, `controle_python_modeles.py` a compté les `null` en Python et
// écrit les LIMITES et le verdict attendus (donnees/modeles_liste.inc) ; ici le vrai lecteur est rejoué dessus : si la table, le
// lecteur ou les réponses ne disent plus la même chose, un test échoue. Cas limites écrits à la main.
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string_view>

#include "ecran/fournisseurs_meteo.hpp"
#include "ecran/meteo_requete.hpp"
#include "ecran/meteo_riche.hpp"
#include "mini.hpp"

using namespace ecran::meteo;
using namespace ecran::meteo::fournisseurs;
using sv = std::string_view;

namespace {

struct Essai {
    sv id;
    sv ville;
    sv pays;
    std::int64_t recu;
    std::uint8_t limites; // vues par Python dans les `null` de la réponse
    bool accepte;         // Python : le lecteur doit garder la réponse
    int heures;           // -1 : réponse refusée
    int jours;
    int quarts;
    sv json;
};

constexpr Essai ESSAIS[] = {
#include "donnees/modeles_liste.inc"
};
constexpr std::size_t NB_ESSAIS = sizeof(ESSAIS) / sizeof(ESSAIS[0]);

constexpr const Fournisseur &entree(sv id) { return *trouver(id); }

constexpr bool liste_est(const Liste &l, std::initializer_list<sv> ids) {
    if (l.n != ids.size())
        return false;
    std::size_t k = 0;
    for (const sv id : ids) {
        if (TABLE[l.indices[k++]].id != id)
            return false;
    }
    return true;
}

// ---- la table face aux réponses réelles ---------------------------------------------------------------------------------

constexpr int chaque_reponse_reelle_a_une_entree_et_les_limites_de_la_table() {
    VERIFIER(NB_ESSAIS == 29);
    for (const Essai &e : ESSAIS) {
        const Fournisseur *f = trouver(e.id);
        VERIFIER(f != nullptr);
        VERIFIER(f->limites == e.limites); // la table dit exactement ce que la réponse réelle montre d'absent
        VERIFIER(concerne_le_pays(*f, e.pays));
    }
    return 0;
}

constexpr int le_lecteur_et_la_table_sont_d_accord_sur_chaque_reponse_reelle() {
    for (const Essai &e : ESSAIS) {
        const AnalyseRiche a = analyser_riche(e.json, e.recu);
        VERIFIER(a.ok() == e.accepte);                   // le vrai lecteur fait ce que Python a prévu
        VERIFIER(proposable(entree(e.id)) == e.accepte); // et la table propose exactement ce qu'il garde
        VERIFIER(proposable(entree(e.id)) == a.ok());
    }
    return 0;
}

constexpr int une_source_proposee_rend_le_courant_24_heures_7_jours_et_les_quarts_d_heure() {
    std::size_t gardees = 0;
    for (const Essai &e : ESSAIS) {
        if (!e.accepte)
            continue;
        ++gardees;
        const AnalyseRiche a = analyser_riche(e.json, e.recu);
        VERIFIER(a.ok());
        VERIFIER(a.diag_heures.ok() && a.diag_jours.ok() && a.diag_quarts.ok()); // aucune section vidée en silence
        VERIFIER(a.meteo.riche.nb_heures == e.heures && e.heures == 24);
        VERIFIER(a.meteo.releve.nb_jours == e.jours && e.jours == 7);
        VERIFIER(a.meteo.riche.nb_jours == 7); // détail des jours : l'ensoleillement est là
        VERIFIER(a.meteo.riche.nb_quarts == e.quarts && e.quarts == 9);
    }
    VERIFIER(gardees == 17); // auto, MET Norway ×6, KNMI ×3, NOAA ×7 : si ce nombre change, relire la liste proposée
    return 0;
}

constexpr int toute_source_refusee_a_une_raison_dans_ses_limites() {
    // Aucune source n'est refusée « pour rien » : chaque refus s'explique par une limite que le lecteur ne tolère pas.
    std::size_t refusees = 0;
    for (const Essai &e : ESSAIS) {
        if (e.accepte)
            continue;
        ++refusees;
        VERIFIER((e.limites & static_cast<std::uint8_t>(~LECTEUR_TOLERE)) != 0);
        VERIFIER(!analyser_riche(e.json, e.recu).ok());
    }
    VERIFIER(refusees == 12);
    return 0;
}

constexpr int chaque_pays_annonce_par_la_table_a_sa_reponse_reelle() {
    // « pays » ne liste que des pays où une capture a rendu quelque chose : jamais un code deviné.
    for (const Fournisseur &f : TABLE) {
        if (f.famille != Famille::OpenMeteo)
            continue; // MET Norway et le NWS : leurs pays sont comparés à leurs captures dans test_meteo_metno.cpp et test_meteo_nws.cpp
        for (std::size_t i = 0; i < f.pays.size(); i += 3) {
            const sv code = f.pays.substr(i, 2);
            bool vu = false;
            for (const Essai &e : ESSAIS)
                vu = vu || (e.id == f.id && e.pays == code);
            VERIFIER(vu);
        }
    }
    return 0;
}

constexpr int la_source_mondiale_est_vue_sur_cinq_continents() {
    std::size_t pays_vus = 0;
    bool us = false, fr = false, jp = false, au = false, br = false, za = false, ca = false;
    for (const Essai &e : ESSAIS) {
        if (e.id != "noaa")
            continue;
        ++pays_vus;
        VERIFIER(e.accepte && e.limites == 0 && e.json.size() > 4000);
        us = us || e.pays == "US";
        fr = fr || e.pays == "FR";
        jp = jp || e.pays == "JP";
        au = au || e.pays == "AU";
        br = br || e.pays == "BR";
        za = za || e.pays == "ZA";
        ca = ca || e.pays == "CA";
    }
    VERIFIER(pays_vus == 7 && us && fr && jp && au && br && za && ca); // Amérique ×2, Europe, Asie, Océanie, Afrique
    return 0;
}

// ---- la table elle-même -------------------------------------------------------------------------------------------------

constexpr int ordre_et_identifiants_figes() {
    // L'ordre sert d'indice d'option dans le registre des réglages et l'identifiant est gardé en mémoire permanente : rien ne
    // bouge, on ajoute à la fin (et on met cette liste à jour en le sachant).
    constexpr sv IDS[] = {"auto", "meteofrance", "dwd",   "metoffice", "metno",     "dmi",
                          "knmi", "meteosuisse", "jma",   "gem",       "geosphere", "arpae",
                          "chmi", "noaa",        "ecmwf", "metnorway", "nws"};
    VERIFIER(NOMBRE == sizeof(IDS) / sizeof(IDS[0]));
    for (std::size_t i = 0; i < NOMBRE; ++i) {
        VERIFIER(TABLE[i].id == IDS[i]);
        VERIFIER(indice_de(IDS[i]) == i);
    }
    VERIFIER(INDICE_AUTO == 0 && indice_de("inconnu") == NOMBRE && indice_de("") == NOMBRE &&
             indice_de("Auto") == NOMBRE);
    return 0;
}

constexpr int seuls_les_modeles_verifies_sont_dans_la_table() {
    // Chaque valeur de `models=` de la table a été capturée avec la requête complète (docs/mesures/meteo-modeles.md).
    constexpr sv VERIFIES[] = {"meteofrance_seamless",
                               "icon_seamless",
                               "ukmo_seamless",
                               "metno_seamless",
                               "dmi_seamless",
                               "knmi_seamless",
                               "meteoswiss_icon_seamless",
                               "jma_seamless",
                               "gem_seamless",
                               "geosphere_seamless",
                               "italia_meteo_arpae_icon_2i",
                               "chmi_aladin_seamless",
                               "gfs_seamless",
                               "ecmwf_ifs025"};
    for (std::size_t i = 1; i < NOMBRE; ++i) {
        if (TABLE[i].famille != Famille::OpenMeteo) {
            VERIFIER(TABLE[i].modeles.empty()); // un service lu directement n'a pas de `models=`
            continue;
        }
        bool connu = false;
        for (const sv m : VERIFIES)
            connu = connu || TABLE[i].modeles == m;
        VERIFIER(connu);
    }
    return 0;
}

constexpr int les_modeles_ecartes_ne_sont_pas_dans_la_table() {
    // `kma_seamless` et `bom_access_global` rendent des sections vides (essai du 07/11/2026) ; les variantes `icon_global`,
    // `icon_eu`, `icon_d2` sont proscrites (consigne d'Axel) ; `ncep_gfs013` double `gfs_seamless` en moins complet ;
    // `cma_grapes_global` n'a ni UV ni rafales.
    constexpr sv ECARTES[] = {"kma_seamless", "bom_access_global", "icon_global",       "icon_eu",
                              "icon_d2",      "ncep_gfs013",       "cma_grapes_global", "best_match"};
    for (const sv m : ECARTES) {
        for (const Fournisseur &f : TABLE)
            VERIFIER(f.modeles != m);
    }
    // Jamais une liste de modèles (« a,b ») : Open-Meteo renommerait les variables de la réponse.
    for (const Fournisseur &f : TABLE) {
        for (const char c : f.modeles)
            VERIFIER(c != ',');
    }
    return 0;
}

constexpr int le_modele_le_plus_long_atteint_la_borne_de_l_adresse() {
    std::size_t plus_long = 0;
    for (const Fournisseur &f : TABLE)
        plus_long = f.modeles.size() > plus_long ? f.modeles.size() : plus_long;
    VERIFIER(plus_long ==
             MODELES_MAX); // si la borne est trop grande, l'adresse gaspille de la place ; trop petite : refus
    return 0;
}

constexpr int chaque_source_donne_une_adresse_qui_tient_dans_le_tampon_d_envoi() {
    for (const Fournisseur &f : TABLE) {
        if (f.famille != Famille::OpenMeteo)
            continue; // les services lus directement ont leurs adresses (test_requete_fournisseurs.cpp)
        char t[TAILLE_REQUETE_MAX] = {};
        const std::size_t n = construire_requete(-9000000, -18000000, t, sizeof(t), f.modeles);
        VERIFIER(n != 0 && n < TAILLE_REQUETE_MAX);
        // La première ligne HTTP (« GET /chemin HTTP/1.1\r\n ») doit tenir dans `buffer_size_tx` = 1024 avec 100 octets de marge.
        constexpr std::size_t SCHEMA_ET_HOTE = sv("https://api.open-meteo.com").size();
        VERIFIER(sv("GET ").size() + (n - SCHEMA_ET_HOTE) + sv(" HTTP/1.1\r\n").size() + 100 <= 1024);
        if (f.modeles.empty())
            VERIFIER(sv(t, n).find("models") == sv::npos); // « Automatique » : rien de plus
        else // le modèle est la dernière chose de l'adresse, derrière « &models= »
            VERIFIER(sv(t + n - f.modeles.size() - 8, 8) == sv("&models=") &&
                     sv(t + n - f.modeles.size(), f.modeles.size()) == f.modeles);
    }
    return 0;
}

constexpr int automatique_ne_demande_aucun_modele() {
    VERIFIER(modeles_de("auto").empty());
    VERIFIER(modeles_de("inconnu").empty()); // un identifiant inconnu (NVS ancienne ou abîmée) = comportement d'avant
    VERIFIER(modeles_de("").empty());
    VERIFIER(modeles_de("metno") == "metno_seamless");
    VERIFIER(modeles_de("noaa") == "gfs_seamless");
    VERIFIER(modeles_de("Metno").empty()); // la casse compte : l'identifiant est celui de la table
    VERIFIER(entree("auto").libelle_traduit && entree("auto").libelle == "Automatique");
    for (std::size_t i = 1; i < NOMBRE; ++i)
        VERIFIER(!TABLE[i].libelle_traduit); // un nom de service est un nom propre : jamais traduit
    return 0;
}

// ---- les services lus directement (ADR-0014) ------------------------------------------------------------------------------

constexpr int la_famille_d_une_source_vient_de_la_table() {
    VERIFIER(entree("metnorway").famille == Famille::MetNo && entree("nws").famille == Famille::Nws);
    VERIFIER(famille_de("metnorway") == Famille::MetNo && famille_de("nws") == Famille::Nws);
    // « metno » (le modèle de MET Norway servi par Open-Meteo) reste une source d'Open-Meteo : seul « metnorway » est lu en direct
    VERIFIER(famille_de("metno") == Famille::OpenMeteo && famille_de("auto") == Famille::OpenMeteo &&
             famille_de("noaa") == Famille::OpenMeteo);
    // un identifiant inconnu, vide ou d'une autre casse n'est jamais un service direct : on ne tente que ce qu'on connaît
    VERIFIER(famille_de("inconnu") == Famille::OpenMeteo && famille_de("") == Famille::OpenMeteo &&
             famille_de("NWS") == Famille::OpenMeteo && famille_de("MetNorway") == Famille::OpenMeteo);
    std::size_t directes = 0;
    for (std::size_t i = 0; i < NOMBRE; ++i) {
        const Fournisseur &f = TABLE[i];
        if (f.famille == Famille::OpenMeteo) {
            VERIFIER(i == 0 || !f.modeles.empty());
        } else {
            ++directes;
            VERIFIER(f.modeles.empty() && !f.pays.empty() &&
                     f.limites == 0); // pays précis, aucune limite qui refuse la réponse
            VERIFIER(proposable(f, 0));
            VERIFIER(modeles_de(f.id).empty()); // jamais un `models=` dans l'adresse de repli
        }
    }
    VERIFIER(directes == 2);
    return 0;
}

constexpr int l_attribution_nomme_le_service_qui_a_donne_la_meteo() {
    const Attribution o = attribution_de(Famille::OpenMeteo);
    const Attribution m = attribution_de(Famille::MetNo);
    const Attribution n = attribution_de(Famille::Nws);
    VERIFIER(o.id == "open-meteo" && o.nom == "Open-Meteo" && o.lien == "https://open-meteo.com/");
    VERIFIER(m.id == "met-norway" && m.nom == "MET Norway" && m.lien == "https://api.met.no/");
    VERIFIER(n.id == "nws" && n.nom == "NWS" && n.lien == "https://www.weather.gov/");
    for (const Attribution &a : {o, m, n}) {
        VERIFIER(a.lien.size() > 12 && a.lien.substr(0, 8) == "https://" && a.lien.back() == '/');
        VERIFIER(a.nom.size() >= 3);
    }
    VERIFIER(o.nom != m.nom && m.nom != n.nom && o.nom != n.nom);
    VERIFIER(o.id != m.id && m.id != n.id && o.id != n.id);
    for (const Attribution &a : {o, m, n}) {
        VERIFIER(!a.id.empty());
        for (const char c : a.id)
            VERIFIER((c >= 'a' && c <= 'z') || c == '-'); // un identifiant d'API : minuscules et tirets
    }
    return 0;
}

// ---- les pays -----------------------------------------------------------------------------------------------------------

constexpr int code_pays_reconnu_a_la_lettre_pres() {
    VERIFIER(fournisseurs::detail::code_dans("FR RE", "FR") && fournisseurs::detail::code_dans("FR RE", "RE"));
    VERIFIER(fournisseurs::detail::code_dans("NO SE FI DK IS EE", "EE") &&
             fournisseurs::detail::code_dans("NO SE FI DK IS EE", "NO"));
    VERIFIER(fournisseurs::detail::code_dans("NO SE FI DK IS EE", "fi"));  // casse indifférente
    VERIFIER(!fournisseurs::detail::code_dans("NO SE FI DK IS EE", "EF")); // à cheval sur « SE FI »
    VERIFIER(!fournisseurs::detail::code_dans("NO SE FI DK IS EE", "E "));
    VERIFIER(!fournisseurs::detail::code_dans("NO SE FI DK IS EE", " F"));
    VERIFIER(!fournisseurs::detail::code_dans("NO SE FI DK IS EE", "ES"));
    VERIFIER(!fournisseurs::detail::code_dans("FR RE", "F") && !fournisseurs::detail::code_dans("FR RE", "FRA") &&
             !fournisseurs::detail::code_dans("FR RE", ""));
    VERIFIER(!fournisseurs::detail::code_dans("", "FR") && !fournisseurs::detail::code_dans("FR", "12") &&
             !fournisseurs::detail::code_dans("FR", "F-"));
    VERIFIER(fournisseurs::detail::code_dans("FR", "FR") && !fournisseurs::detail::code_dans("FR", "RF"));
    VERIFIER(!fournisseurs::detail::code_dans("FRA RE", "RA")); // dans la liste, mais pas au début d'un code
    VERIFIER(!fournisseurs::detail::code_dans("AFR RE", "AF")); // dans la liste, mais pas à la fin d'un code
    VERIFIER(!fournisseurs::detail::code_dans("1A", "1A") &&
             !fournisseurs::detail::code_dans("A1", "A1")); // pas des lettres
    VERIFIER(fournisseurs::detail::pays_bien_ecrit("") && fournisseurs::detail::pays_bien_ecrit("FR") &&
             fournisseurs::detail::pays_bien_ecrit("FR RE"));
    VERIFIER(!fournisseurs::detail::pays_bien_ecrit("fr") && !fournisseurs::detail::pays_bien_ecrit("FR,RE") &&
             !fournisseurs::detail::pays_bien_ecrit("FR  RE"));
    VERIFIER(!fournisseurs::detail::pays_bien_ecrit("FR ") && !fournisseurs::detail::pays_bien_ecrit(" FR") &&
             !fournisseurs::detail::pays_bien_ecrit("FRA"));
    return 0;
}

constexpr int liste_proposee_pour_chaque_pays() {
    // Automatique en tête, puis les sources nationales proposables, puis les mondiales proposables.
    VERIFIER(
        liste_est(pour_pays("FR"), {"auto", "noaa"})); // Météo-France n'est pas proposée : pas d'UV (voir plus haut)
    // MET Norway lu directement (« metnorway ») vient après le modèle d'Open-Meteo du même nom (« metno »), avant les mondiales.
    VERIFIER(liste_est(pour_pays("NO"), {"auto", "metno", "metnorway", "noaa"}));
    for (const sv nordique : {sv("FI"), sv("IS"), sv("SJ")}) {
        // « SJ » (Svalbard) : seulement le service direct, le modèle d'Open-Meteo ne le liste pas
        const bool modele = nordique != sv("SJ");
        if (modele)
            VERIFIER(liste_est(pour_pays(nordique), {"auto", "metno", "metnorway", "noaa"}));
        else
            VERIFIER(liste_est(pour_pays(nordique), {"auto", "metnorway", "noaa"}));
    }
    for (const sv nordique : {sv("SE"), sv("DK"), sv("EE")}) // pas encore de capture du service direct dans ces pays
        VERIFIER(liste_est(pour_pays(nordique), {"auto", "metno", "noaa"}));
    for (const sv bnl : {sv("NL"), sv("BE"), sv("LU")})
        VERIFIER(liste_est(pour_pays(bnl), {"auto", "knmi", "noaa"}));
    VERIFIER(liste_est(pour_pays("US"), {"auto", "nws", "noaa"}));
    VERIFIER(liste_est(pour_pays("PR"), {"auto", "nws", "noaa"}));
    VERIFIER(liste_est(pour_pays("DE"), {"auto", "noaa"}));
    VERIFIER(liste_est(pour_pays("JP"), {"auto", "noaa"}));
    return 0;
}

constexpr int pays_inconnu_ou_mal_forme_ne_donne_que_les_mondiales() {
    VERIFIER(liste_est(pour_pays(""), {"auto", "noaa"})); // ville sans pays connu (réglage ancien)
    VERIFIER(liste_est(pour_pays("ZZ"), {"auto", "noaa"}));
    VERIFIER(liste_est(pour_pays("FRA"), {"auto", "noaa"})); // trois lettres : pas un code ISO alpha-2
    VERIFIER(liste_est(pour_pays("F"), {"auto", "noaa"}));
    VERIFIER(liste_est(pour_pays("N0"), {"auto", "noaa"}));
    VERIFIER(liste_est(pour_pays("no"), {"auto", "metno", "metnorway", "noaa"})); // casse indifférente
    VERIFIER(liste_est(pour_pays("us"), {"auto", "nws", "noaa"}));
    return 0;
}

constexpr int tolerer_une_limite_fait_apparaitre_les_sources_qui_n_ont_que_celle_la() {
    // Le jour où le lecteur ne refusera plus un UV vide : DWD, DMI… apparaissent sans autre changement.
    constexpr std::uint8_t SANS_UV_TOLERE = LECTEUR_TOLERE | limite::SANS_UV;
    VERIFIER(liste_est(pour_pays("DE", SANS_UV_TOLERE), {"auto", "dwd", "noaa", "ecmwf"}));
    VERIFIER(liste_est(pour_pays("DK", SANS_UV_TOLERE), {"auto", "metno", "dmi", "noaa", "ecmwf"}));
    VERIFIER(liste_est(pour_pays("CA", SANS_UV_TOLERE), {"auto", "gem", "noaa", "ecmwf"}));
    VERIFIER(liste_est(pour_pays("FR", SANS_UV_TOLERE),
                       {"auto", "noaa", "ecmwf"})); // Météo-France : d'autres limites encore
    VERIFIER(liste_est(pour_pays("GB", SANS_UV_TOLERE),
                       {"auto", "metoffice", "noaa", "ecmwf"})); // ensoleillement déjà toléré
    VERIFIER(!propose_pour("dwd", "DE") && propose_pour("dwd", "DE", SANS_UV_TOLERE));
    // Tout tolérer : toute la table est proposable, pays par pays.
    constexpr std::uint8_t TOUT = 0xFF;
    VERIFIER(liste_est(pour_pays("FR", TOUT), {"auto", "meteofrance", "noaa", "ecmwf"}));
    VERIFIER(liste_est(pour_pays("JP", TOUT), {"auto", "jma", "noaa", "ecmwf"}));
    // Ne rien tolérer : « Automatique » et les sources sans aucune limite seulement.
    VERIFIER(liste_est(pour_pays("NO", 0), {"auto", "metno", "metnorway", "noaa"}));
    VERIFIER(!proposable(entree("metoffice"), 0) && !proposable(entree("metoffice")));
    return 0;
}

constexpr int propose_pour_dit_si_la_source_convient_a_la_ville() {
    VERIFIER(propose_pour("auto", "FR") && propose_pour("auto", "") && propose_pour("auto", "ZZ"));
    VERIFIER(propose_pour("metno", "NO") && propose_pour("metno", "ee"));
    VERIFIER(!propose_pour("metno", "FR") && !propose_pour("metno", "") && !propose_pour("metno", "JP"));
    VERIFIER(propose_pour("noaa", "FR") && propose_pour("noaa", "") && propose_pour("noaa", "AU"));
    VERIFIER(propose_pour("metnorway", "NO") && propose_pour("metnorway", "sj") && !propose_pour("metnorway", "SE") &&
             !propose_pour("metnorway", "") && !propose_pour("metnorway", "US"));
    VERIFIER(propose_pour("nws", "US") && propose_pour("nws", "pr") && !propose_pour("nws", "FR") &&
             !propose_pour("nws", "") && !propose_pour("nws", "CA"));
    VERIFIER(propose_pour("knmi", "BE") && !propose_pour("knmi", "DE"));
    VERIFIER(!propose_pour("dwd", "DE")); // dans la table, pas proposée (UV)
    VERIFIER(!propose_pour("meteofrance", "FR"));
    VERIFIER(!propose_pour("inconnu", "FR") && !propose_pour("", "FR") && !propose_pour("NOAA", "FR"));
    return 0;
}

// Ce que l'appareil garde quand la ville change ou quand la mémoire permanente est relue : la même source si elle convient encore au
// pays, sinon « Automatique », jamais un identifiant qui ne couvre pas la ville (et jamais une erreur muette : l'appelant compare).
constexpr int source_valable_pour_garde_la_source_ou_remet_automatique() {
    VERIFIER(source_valable_pour("noaa", "FR") == "noaa"); // un modèle mondial convient à tous les pays
    VERIFIER(source_valable_pour("noaa", "") == "noaa");   // pays inconnu : le modèle mondial reste
    VERIFIER(source_valable_pour("knmi", "NL") == "knmi" && source_valable_pour("knmi", "BE") == "knmi");
    VERIFIER(source_valable_pour("metno", "se") == "metno"); // casse indifférente
    VERIFIER(source_valable_pour("knmi", "FR") == "auto");   // ne couvre pas la France : retour à « Automatique »
    VERIFIER(source_valable_pour("metno", "NL") == "auto");
    VERIFIER(source_valable_pour("nws", "US") == "nws" &&
             source_valable_pour("nws", "FR") == "auto"); // un service lu directement
    VERIFIER(source_valable_pour("metnorway", "FI") == "metnorway" && source_valable_pour("metnorway", "FR") == "auto");
    VERIFIER(source_valable_pour("knmi", "") == "auto"); // pays inconnu : seule une source mondiale reste
    VERIFIER(source_valable_pour("auto", "FR") == "auto" && source_valable_pour("auto", "") == "auto");
    VERIFIER(source_valable_pour("inconnu", "FR") == "auto" &&
             source_valable_pour("", "NL") == "auto");     // mémoire abîmée
    VERIFIER(source_valable_pour("NOAA", "FR") == "auto"); // la casse de l'identifiant compte
    VERIFIER(source_valable_pour("meteofrance", "FR") ==
             "auto"); // dans la table, pas proposée (le lecteur refuse sa réponse)
    // Le jour où le lecteur tolérerait ce qui lui manque, la source resterait : la tolérance passée est bien suivie.
    constexpr std::uint8_t TOUT = limite::SANS_UV | limite::SANS_RAFALES | limite::SANS_PROBA | limite::JOURS_COURTS |
                                  limite::SANS_ENSOLEILLEMENT;
    VERIFIER(source_valable_pour("meteofrance", "FR", TOUT) == "meteofrance");
    VERIFIER(source_valable_pour("meteofrance", "DE", TOUT) ==
             "auto"); // le pays reste jugé, même avec un lecteur qui tolère tout
    // Quel que soit le texte reçu, le résultat est l'identifiant d'une entrée de la table.
    for (const sv recu : {sv("auto"), sv("knmi"), sv("noaa"), sv("n_importe_quoi"), sv("")}) {
        for (const sv p : {sv(""), sv("FR"), sv("NO"), sv("NL"), sv("JP")})
            VERIFIER(trouver(source_valable_pour(recu, p)) != nullptr);
    }
    return 0;
}

constexpr int la_liste_n_a_ni_doublon_ni_trou_pour_aucun_pays() {
    constexpr sv PAYS[] = {"",   "FR", "DE", "GB", "NO", "SE", "FI", "DK", "IS", "EE", "NL", "BE", "LU", "CH", "SJ",
                           "PR", "JP", "CA", "AT", "IT", "CZ", "US", "AU", "BR", "ZA", "TR", "ES", "RE", "ZZ", "fr"};
    for (const sv p : PAYS) {
        const Liste l = pour_pays(p);
        VERIFIER(l.n >= 2 && l.n <= NOMBRE);
        VERIFIER(l.indices[0] == INDICE_AUTO);
        VERIFIER(TABLE[l.indices[l.n - 1]].pays.empty()); // la liste finit par une source mondiale
        for (std::size_t a = 0; a < l.n; ++a) {
            VERIFIER(l.indices[a] < NOMBRE);
            VERIFIER(a == 0 || proposable(TABLE[l.indices[a]]));
            for (std::size_t b = a + 1; b < l.n; ++b)
                VERIFIER(l.indices[a] != l.indices[b]);
        }
        // Les nationales d'abord, les mondiales ensuite : jamais une nationale après une mondiale.
        bool mondiale_vue = false;
        for (std::size_t a = 1; a < l.n; ++a) {
            const bool mondiale = TABLE[l.indices[a]].pays.empty();
            VERIFIER(!(mondiale_vue && !mondiale));
            mondiale_vue = mondiale_vue || mondiale;
        }
        // Chaque identifiant de la liste est valable pour ce pays, et seulement eux.
        for (std::size_t i = 0; i < NOMBRE; ++i) {
            bool dans_liste = false;
            for (std::size_t a = 0; a < l.n; ++a)
                dans_liste = dans_liste || l.indices[a] == i;
            VERIFIER(propose_pour(TABLE[i].id, p) == dans_liste);
        }
    }
    return 0;
}

#define LISTE(X)                                                                                                       \
    X(chaque_reponse_reelle_a_une_entree_et_les_limites_de_la_table)                                                   \
    X(le_lecteur_et_la_table_sont_d_accord_sur_chaque_reponse_reelle)                                                  \
    X(une_source_proposee_rend_le_courant_24_heures_7_jours_et_les_quarts_d_heure)                                     \
    X(toute_source_refusee_a_une_raison_dans_ses_limites)                                                              \
    X(chaque_pays_annonce_par_la_table_a_sa_reponse_reelle)                                                            \
    X(la_source_mondiale_est_vue_sur_cinq_continents)                                                                  \
    X(ordre_et_identifiants_figes)                                                                                     \
    X(seuls_les_modeles_verifies_sont_dans_la_table)                                                                   \
    X(les_modeles_ecartes_ne_sont_pas_dans_la_table)                                                                   \
    X(le_modele_le_plus_long_atteint_la_borne_de_l_adresse)                                                            \
    X(chaque_source_donne_une_adresse_qui_tient_dans_le_tampon_d_envoi)                                                \
    X(automatique_ne_demande_aucun_modele)                                                                             \
    X(la_famille_d_une_source_vient_de_la_table)                                                                       \
    X(l_attribution_nomme_le_service_qui_a_donne_la_meteo)                                                             \
    X(code_pays_reconnu_a_la_lettre_pres)                                                                              \
    X(liste_proposee_pour_chaque_pays)                                                                                 \
    X(pays_inconnu_ou_mal_forme_ne_donne_que_les_mondiales)                                                            \
    X(tolerer_une_limite_fait_apparaitre_les_sources_qui_n_ont_que_celle_la)                                           \
    X(propose_pour_dit_si_la_source_convient_a_la_ville)                                                               \
    X(source_valable_pour_garde_la_source_ou_remet_automatique)                                                        \
    X(la_liste_n_a_ni_doublon_ni_trou_pour_aucun_pays)

TESTS_A_LA_COMPILATION(LISTE)

} // namespace

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
