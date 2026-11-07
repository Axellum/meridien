// Noyau pur : la logique de l'API de la page de réglages (docs/api-reglages.md), sans réseau ni ESP-IDF.
// Le serveur du firmware (main/serveur_web.cpp) ne fait que lire une requête HTTP, appeler `Api::traiter` et écrire la réponse ;
// tout ce qui décide (appairage, 401, validations, forme des réponses, phrases dans la langue de l'écran) est ici et se teste
// sur le PC avec un faux « hôte » (test/host/test_api_reglages.cpp).
//
// SOUS-ENSEMBLE V1 (premier firmware qui sert la page, jalon « réglages ») :
//   réel   : /api/status, /api/pair (POST, DELETE), /api/system, /api/location (GET, PUT), /api/display (GET, PUT), /api/alerts (GET, PUT),
//            /api/weather, /api/geocode, /api/wifi (GET en lecture seule)
//   vide   : /api/services (rien de réglé), /api/presets (aucun), /api/update (inactif) : la page s'affiche sans erreur
//   501    : changer de réseau Wi-Fi, enregistrer ou tester un service, export / import, mise à jour, retour arrière.
//            Ces routes existent dans le contrat ; elles répondent « pas encore disponible » dans la langue de l'écran.
//
// Règles du contrat tenues ici : une réponse d'erreur a toujours la forme {"erreur":{"id","message"[,"champ"]}} ; `id` est stable
// et jamais traduit, `message` est une phrase de l'écran dans sa langue ; aucune clé API n'existe dans ce sous-ensemble (aucune
// route n'en lit ni n'en écrit) ; un corps de plus de 4 Ko -> 413 ; une route protégée sans jeton valide -> 401.
//
// L'hôte (`Hote`) fournit (types plus bas) :
//   reglages::Affichage affichage() const;                 bool enregistrer_affichage(const reglages::Affichage &);
//   reglages::Alertes alertes() const;                     bool enregistrer_alertes(const reglages::Alertes &);
//   bool zone_lire(reglages::Zone &);                      bool zone_enregistrer(const reglages::Zone &);
//   std::string_view source_meteo();                       bool source_meteo_enregistrer(std::string_view id);
//   bool source_meteo_retablie();                          // source météo en service (« auto », « knmi »), ADR-0010
//   bool meteo_lire(MeteoApercu &);                        InfoSysteme systeme();      InfoWifi wifi();
//   Telechargement telecharger(std::string_view url, char *tampon, std::size_t capacite, std::size_t &taille);
//   std::int64_t maintenant_ms();                          void aleatoire(std::uint8_t *sortie, std::size_t n);
// C++17 standard, aucun include ESP-IDF, évaluable à la compilation.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/api_json.hpp"
#include "ecran/appairage.hpp"
#include "ecran/fournisseurs_meteo.hpp"
#include "ecran/fuseaux.hpp"
#include "ecran/geocodage.hpp"
#include "ecran/i18n.hpp"
#include "ecran/json_mini.hpp"
#include "ecran/meteo_wmo.hpp"
#include "ecran/reglages_alertes.hpp"
#include "ecran/reglages_valeurs.hpp"

namespace ecran::api {

inline constexpr std::size_t CORPS_MAX = 4096; // octets (api-reglages.md §1 règle 8)
inline constexpr int RESULTATS_GEOCODAGE_MAX = 8;
// La page et l'écran de l'appareil affichent le même nombre de lieux (geocodage.hpp).
static_assert(static_cast<std::size_t>(RESULTATS_GEOCODAGE_MAX) == geocodage::LIEUX_MAX);
inline constexpr std::size_t ADRESSE_GEOCODAGE_MAX =
    1200; // 80 caractères de 4 octets, chacun encodé en %XX, plus le reste

enum class Methode : std::uint8_t { Get, Post, Put, Delete, Autre };

struct Requete {
    Methode methode = Methode::Get;
    std::string_view chemin{};  // « /api/location », sans la requête
    std::string_view requete{}; // ce qui suit « ? », non décodé
    std::string_view corps{};   // corps complet
    std::string_view jeton{};   // ce qui suit « Bearer » dans l'en-tête Authorization, sinon vide
};

struct Reponse {
    int statut = 500;
    std::string_view corps{}; // dans le tampon de sortie ; vide pour 204
    int reessayer_dans = 0;   // secondes, seulement pour 429
};

struct Tampons {
    char *sortie = nullptr; // réponse JSON (6 Ko suffisent pour 8 lieux)
    std::size_t cap_sortie = 0;
    char *travail = nullptr; // réponse brute du géocodage (16 Ko)
    std::size_t cap_travail = 0;
};

struct InfoSysteme {
    std::string_view nom{};    // « ecran-f6b6 »
    std::string_view modele{}; // nom de la carte sur laquelle tourne le firmware (« M5Stack Tab5 », « Waveshare 7B »)
    std::string_view version{};
    std::string_view compile_le{}; // « 2026-11-06T11:20:00Z », ou vide si inconnu
    std::string_view partition{};
    bool validee = true;
    std::int64_t depuis_secondes = 0;
    std::int64_t memoire_libre = 0;
    std::int64_t heure_epoch = 0;
    bool heure_synchronisee = false;
    std::string_view fuseau{};
    bool internet = false;
};

enum class EtatWifi : std::uint8_t { Connecte, Connexion, Echec, Deconnecte };

struct InfoWifi {
    EtatWifi etat = EtatWifi::Deconnecte;
    char ssid[33]{};
    int force_dbm = 0;
    char ip[16]{};
};

struct MeteoApercu {
    bool valide = false;
    bool perimee = false;
    std::int64_t maj_epoch_s = 0;
    double temperature_c = 0;
    int code_wmo = 0;
    bool jour = true;
    // Le service qui a RÉELLEMENT donné cette météo (pas forcément la source choisie : après un échec du service direct, c'est Open-Meteo).
    meteo::fournisseurs::Famille origine = meteo::fournisseurs::Famille::OpenMeteo;
};

enum class Telechargement : std::uint8_t { Ok, SansInternet, Echec };

// Niveau d'affichage 0 à 4 pour une puissance en dBm (api-reglages.md §4). Seuils usuels, non mesurés sur l'appareil.
constexpr int niveau_wifi(int dbm) noexcept {
    if (dbm >= -55)
        return 4;
    if (dbm >= -65)
        return 3;
    if (dbm >= -75)
        return 2;
    if (dbm >= -85)
        return 1;
    return 0;
}

// Écrit `modele` dans `e` en remplaçant « {nom} » par `valeur`.
constexpr void remplacer(std::string_view modele, std::string_view nom, std::string_view valeur, Ecrivain &e) noexcept {
    std::size_t i = 0;
    while (i < modele.size()) {
        if (modele[i] == '{' && modele.size() - i >= nom.size() + 2 && modele.substr(i + 1, nom.size()) == nom &&
            modele[i + 1 + nom.size()] == '}') {
            e.brut(valeur);
            i += nom.size() + 2;
        } else {
            e.octet(modele[i]);
            ++i;
        }
    }
}

// « Lyon, Auvergne-Rhône-Alpes, France » : les parties non vides, séparées par « , » (aussi utilisé par la lecture de la ville).
using geocodage::ecrire_libelle;

// Réponse du géocodage d'Open-Meteo -> {"resultats":[...]} du contrat (api-reglages.md §5). La lecture, les règles d'écart et
// l'écriture vivent dans le noyau de la recherche de lieu (geocodage.hpp), partagé avec l'écran de l'appareil. Rend faux si le
// texte n'est pas un objet JSON, ou si `e` déborde.
constexpr bool convertir_geocodage(std::string_view texte, Ecrivain &e) noexcept {
    return geocodage::convertir(texte, e);
}

template <class Hote> class Api {
  public:
    explicit constexpr Api(Hote &hote) noexcept : hote_(hote) {}

    constexpr appairage::Gestionnaire &appairage() noexcept { return appairage_; }
    constexpr const appairage::Gestionnaire &appairage() const noexcept { return appairage_; }

    // Traite une requête de /api/. La réponse est écrite dans `t.sortie`.
    constexpr Reponse traiter(const Requete &rq, const Tampons &t) noexcept {
        if (rq.corps.size() > CORPS_MAX)
            return erreur(t, 413, "trop_gros", "Ce fichier est trop gros pour l’appareil.");

        // --- routes publiques ---
        if (est(rq, Methode::Get, "/api/status"))
            return statut(rq, t);
        if (est(rq, Methode::Post, "/api/pair"))
            return appairer(rq, t);

        // --- tout le reste exige un jeton de session ---
        if (!appairage_.jeton_valide(rq.jeton))
            return erreur(t, 401, "non_appaire", "Entrez le code affiché sur l’écran de l’appareil.");

        if (est(rq, Methode::Delete, "/api/pair")) {
            appairage_.fermer(rq.jeton);
            Reponse r;
            r.statut = 204;
            return r;
        }
        if (est(rq, Methode::Get, "/api/system"))
            return systeme(t);
        if (est(rq, Methode::Get, "/api/location"))
            return zone_lire(t);
        if (est(rq, Methode::Put, "/api/location"))
            return zone_ecrire(rq, t);
        if (est(rq, Methode::Get, "/api/display"))
            return affichage_lire(t);
        if (est(rq, Methode::Put, "/api/display"))
            return affichage_ecrire(rq, t);
        if (est(rq, Methode::Get, "/api/alerts"))
            return alertes_lire(t);
        if (est(rq, Methode::Put, "/api/alerts"))
            return alertes_ecrire(rq, t);
        if (est(rq, Methode::Get, "/api/weather"))
            return meteo(t);
        if (est(rq, Methode::Get, "/api/geocode"))
            return geocoder(rq, t);
        if (est(rq, Methode::Get, "/api/wifi"))
            return wifi(t);
        if (est(rq, Methode::Get, "/api/services"))
            return services(t);
        if (est(rq, Methode::Get, "/api/presets"))
            return corps_json(t, 200, [](Ecrivain &e) {
                e.ouvrir_objet();
                for (std::string_view id : {"stt", "llm", "tts"}) {
                    e.cle(id);
                    e.ouvrir_tableau();
                    e.fermer_tableau();
                }
                e.fermer_objet();
            });
        if (est(rq, Methode::Get, "/api/update"))
            return mise_a_jour(t);
        if (hors_perimetre(rq))
            return erreur(t, 501, "non_disponible", "Cette fonction n’est pas encore disponible sur cet écran.");
        return erreur(t, 404, "inconnue", "Cette page n’existe pas.");
    }

  private:
    static constexpr bool est(const Requete &rq, Methode m, std::string_view chemin) noexcept {
        return rq.methode == m && rq.chemin == chemin;
    }

    // Routes du contrat que ce firmware ne fait pas encore.
    static constexpr bool hors_perimetre(const Requete &rq) noexcept {
        const std::string_view c = rq.chemin;
        const Methode m = rq.methode;
        if (c == "/api/wifi/scan")
            return m == Methode::Get || m == Methode::Post;
        if (c == "/api/wifi")
            return m == Methode::Put;
        if (c == "/api/services/stt" || c == "/api/services/llm" || c == "/api/services/tts")
            return m == Methode::Put;
        if (c == "/api/services/stt/test" || c == "/api/services/llm/test" || c == "/api/services/tts/test")
            return m == Methode::Get || m == Methode::Post;
        if (c == "/api/settings/export")
            return m == Methode::Get;
        if (c == "/api/settings/import")
            return m == Methode::Post;
        if (c == "/api/update" || c == "/api/update/rollback")
            return m == Methode::Post;
        return false;
    }

    constexpr i18n::Langue langue() const noexcept { return hote_.affichage().langue_ecran; }
    constexpr std::string_view tr(std::string_view fr) const noexcept { return i18n::tr(fr, langue()); }

    // --- construction des réponses ---

    template <class F> constexpr Reponse corps_json(const Tampons &t, int statut, F &&ecrire) noexcept {
        Ecrivain e(t.sortie, t.cap_sortie);
        ecrire(e);
        if (!e.ok())
            return panne(t);
        Reponse r;
        r.statut = statut;
        r.corps = e.vue();
        return r;
    }

    // Défaut interne : une réponse 500 minimale, ou rien si même elle ne tient pas.
    constexpr Reponse panne(const Tampons &t) noexcept {
        Ecrivain e(t.sortie, t.cap_sortie);
        ecrire_erreur(e, "interne", tr("L’appareil a rencontré un problème. Réessayez."), {}, 0);
        Reponse r;
        r.statut = 500;
        if (e.ok())
            r.corps = e.vue();
        return r;
    }

    static constexpr void ecrire_erreur(Ecrivain &e, std::string_view id, std::string_view message,
                                        std::string_view champ, int reessayer_dans) noexcept {
        e.ouvrir_objet();
        e.cle("erreur");
        e.ouvrir_objet();
        e.champ("id", id);
        e.champ("message", message);
        if (!champ.empty())
            e.champ("champ", champ);
        if (reessayer_dans > 0)
            e.champ_entier("reessayer_dans", reessayer_dans);
        e.fermer_objet();
        e.fermer_objet();
    }

    constexpr Reponse erreur(const Tampons &t, int statut, std::string_view id, std::string_view fr,
                             std::string_view champ = {}) noexcept {
        Ecrivain e(t.sortie, t.cap_sortie);
        ecrire_erreur(e, id, tr(fr), champ, 0);
        if (!e.ok())
            return panne(t);
        Reponse r;
        r.statut = statut;
        r.corps = e.vue();
        return r;
    }

    // Lit un corps JSON qui doit être un objet ; sinon écrit la réponse 400 dans `refus` et rend faux.
    constexpr bool lire_objet(const Requete &rq, const Tampons &t, json::Valeur &objet, Reponse &refus) noexcept {
        const json::Resultat lu = json::lire(rq.corps);
        if (!lu.ok() || lu.valeur.genre() != json::Genre::Objet) {
            refus = erreur(t, 400, "illisible", "Cette demande n’est pas lisible.");
            return false;
        }
        objet = lu.valeur;
        return true;
    }

    // --- routes ---

    constexpr Reponse statut(const Requete &rq, const Tampons &t) noexcept {
        const InfoSysteme s = hote_.systeme();
        const bool ok = appairage_.jeton_valide(rq.jeton);
        const i18n::Langue l = langue();
        return corps_json(t, 200, [&](Ecrivain &e) {
            e.ouvrir_objet();
            e.champ("nom", s.nom);
            e.champ("modele", s.modele);
            e.champ("version", s.version);
            e.champ("mode", "station");
            e.champ("appairage", ok ? "ok" : "requis");
            e.champ("langue", i18n::code(l));
            e.fermer_objet();
        });
    }

    constexpr Reponse appairer(const Requete &rq, const Tampons &t) noexcept {
        json::Valeur objet;
        Reponse refus;
        if (!lire_objet(rq, t, objet, refus))
            return refus;
        char propose[16] = {};
        const json::Valeur code = objet.champ("code");
        bool tronque = false;
        if (code.genre() == json::Genre::Chaine)
            code.copier_texte(propose, sizeof propose, &tronque);
        const appairage::Resultat r =
            appairage_.essayer(tronque ? std::string_view{} : std::string_view(propose), hote_.maintenant_ms(),
                               [this](std::uint8_t *o, std::size_t n) { hote_.aleatoire(o, n); });
        switch (r.issue) {
        case appairage::Issue::Accepte:
            return corps_json(t, 200, [&](Ecrivain &e) {
                e.ouvrir_objet();
                e.champ("jeton", std::string_view(r.jeton, appairage::JETON_CARACTERES));
                e.fermer_objet();
            });
        case appairage::Issue::Verrouille: {
            Ecrivain e(t.sortie, t.cap_sortie);
            char message[160] = {};
            Ecrivain m(message, sizeof message);
            char n[24] = {};
            if (r.reessayer_dans > 1) {
                remplacer(tr("Trop d’essais. Réessayez dans {n} secondes."), "n", entier_en_texte(r.reessayer_dans, n),
                          m);
            } else {
                m.brut(tr("Trop d’essais. Réessayez dans 1 seconde."));
            }
            ecrire_erreur(e, "verrouille", m.vue(), {}, r.reessayer_dans);
            if (!e.ok() || !m.ok())
                return panne(t);
            Reponse rep;
            rep.statut = 429;
            rep.corps = e.vue();
            rep.reessayer_dans = r.reessayer_dans;
            return rep;
        }
        case appairage::Issue::Refuse:
            break;
        }
        return erreur(t, 403, "code_faux", "Ce code n’est pas le bon. Regardez l’écran de l’appareil.", "code");
    }

    constexpr Reponse systeme(const Tampons &t) noexcept {
        const InfoSysteme s = hote_.systeme();
        return corps_json(t, 200, [&](Ecrivain &e) {
            e.ouvrir_objet();
            e.champ("nom", s.nom);
            e.champ("modele", s.modele);
            e.champ("version", s.version);
            if (!s.compile_le.empty())
                e.champ("compile_le", s.compile_le);
            e.champ("partition", s.partition);
            e.champ_booleen("validee", s.validee);
            e.champ_entier("depuis_secondes", s.depuis_secondes);
            e.champ_entier("memoire_libre", s.memoire_libre);
            e.cle("heure");
            e.ouvrir_objet();
            e.champ_entier("epoch", s.heure_epoch);
            e.champ_booleen("synchronisee", s.heure_synchronisee);
            e.champ("fuseau", s.fuseau);
            e.fermer_objet();
            e.champ_booleen("internet", s.internet);
            e.fermer_objet();
        });
    }

    // Le nom d'une source tel que la page l'affiche : « Automatique » dans la langue de l'écran, un nom de service tel quel.
    constexpr std::string_view nom_de_source(const meteo::fournisseurs::Fournisseur &f) const noexcept {
        return f.libelle_traduit ? tr(f.libelle) : f.libelle;
    }

    // Les champs de la source météo, ajoutés à la réponse de /api/location : celle en service, son nom, ce que le dernier changement
    // de ville a fait d'elle, et la liste que le pays de la ville propose (« Automatique » en tête). Même liste que celle de l'écran.
    constexpr void ecrire_source(Ecrivain &e, std::string_view code_pays) noexcept {
        namespace fm = meteo::fournisseurs;
        const std::size_t i = fm::indice_de(hote_.source_meteo());
        const fm::Fournisseur &courante = fm::TABLE[i < fm::NOMBRE ? i : fm::INDICE_AUTO];
        e.champ("source_meteo", courante.id);
        e.champ("source_meteo_nom", nom_de_source(courante));
        e.champ_booleen("source_meteo_retablie", hote_.source_meteo_retablie());
        e.cle("sources_meteo");
        e.ouvrir_tableau();
        const fm::Liste l = fm::pour_pays(code_pays);
        for (std::size_t k = 0; k < l.n; ++k) {
            e.ouvrir_objet();
            e.champ("id", fm::TABLE[l.indices[k]].id);
            e.champ("nom", nom_de_source(fm::TABLE[l.indices[k]]));
            // `direct` : le service est lu directement (MET Norway, NWS), ce n'est pas un modèle d'Open-Meteo : la page ne lui écrit
            // pas « Modèle : … » (ADR-0014).
            e.champ_booleen("direct", fm::TABLE[l.indices[k]].famille != fm::Famille::OpenMeteo);
            e.fermer_objet();
        }
        e.fermer_tableau();
    }

    constexpr void ecrire_zone(Ecrivain &e, const reglages::Zone &z) noexcept {
        e.ouvrir_objet();
        e.champ("nom", z.nom);
        e.champ("region", z.region);
        e.champ("pays", z.pays);
        e.champ("code_pays", z.code_pays);
        e.champ_decimal("latitude", z.latitude, 5);
        e.champ_decimal("longitude", z.longitude, 5);
        e.champ("fuseau", z.fuseau);
        e.champ("posix", fuseaux::posix_pour(z.fuseau));
        e.cle("libelle");
        char libelle[256] = {};
        Ecrivain l(libelle, sizeof libelle);
        ecrire_libelle(l, z.nom, z.region, z.pays);
        e.chaine(l.vue());
        ecrire_source(e, z.code_pays);
        e.fermer_objet();
    }

    // Pas de ville enregistrée : on le dit, et la source météo reste réglable (sans pays connu : « Automatique » et le modèle mondial).
    constexpr void ecrire_zone_absente(Ecrivain &e) noexcept {
        e.ouvrir_objet();
        e.champ_booleen("definie", false);
        ecrire_source(e, {});
        e.fermer_objet();
    }

    constexpr Reponse zone_lire(const Tampons &t) noexcept {
        reglages::Zone z;
        if (!hote_.zone_lire(z)) {
            return corps_json(t, 200, [&](Ecrivain &e) { ecrire_zone_absente(e); });
        }
        return corps_json(t, 200, [&](Ecrivain &e) { ecrire_zone(e, z); });
    }

    constexpr Reponse zone_ecrire(const Requete &rq, const Tampons &t) noexcept {
        json::Valeur objet;
        Reponse refus;
        if (!lire_objet(rq, t, objet, refus))
            return refus;
        // `source_meteo` seul (sans `nom`) ne touche pas à la ville ; avec une ville, la source est jugée sur le pays de la NOUVELLE ville.
        const json::Valeur source = objet.champ("source_meteo");
        const bool source_seule = source.existe() && !objet.champ("nom").existe();
        reglages::ResultatZone v;
        if (source_seule) {
            if (!hote_.zone_lire(v.zone))
                v.zone =
                    reglages::Zone{}; // pas de ville enregistrée : pas de pays, donc seulement les sources mondiales
        } else {
            v = reglages::valider_zone(objet);
            switch (v.erreur) {
            case reglages::ErreurZone::Nom:
                return erreur(t, 422, "nom_invalide", "Le nom de la ville n’est pas valide.", "nom");
            case reglages::ErreurZone::Coordonnees:
                return erreur(t, 422, "coordonnees_invalides", "Ces coordonnées ne sont pas valides.", "latitude");
            case reglages::ErreurZone::Fuseau:
                return erreur(t, 422, "fuseau_inconnu", "Ce fuseau horaire n’est pas connu de l’appareil.", "fuseau");
            case reglages::ErreurZone::Aucune:
                break;
            }
        }
        std::size_t demandee = meteo::fournisseurs::NOMBRE; // NOMBRE : aucune source demandée
        if (source.existe()) {
            char id[16] = {};
            bool tronque = false;
            if (source.genre() == json::Genre::Chaine)
                source.copier_texte(id, sizeof id, &tronque);
            demandee = source.genre() == json::Genre::Chaine && !tronque ? meteo::fournisseurs::indice_de(id)
                                                                         : meteo::fournisseurs::NOMBRE;
            if (demandee >= meteo::fournisseurs::NOMBRE)
                return erreur(t, 422, "source_inconnue", "Cette source météo n’existe pas.", "source_meteo");
            if (!meteo::fournisseurs::propose_pour(id, v.zone.code_pays))
                return erreur(t, 422, "source_indisponible", "Cette source météo ne couvre pas le pays de la ville.",
                              "source_meteo");
        }
        if (!source_seule && !hote_.zone_enregistrer(v.zone))
            return panne(t);
        if (demandee < meteo::fournisseurs::NOMBRE &&
            !hote_.source_meteo_enregistrer(meteo::fournisseurs::TABLE[demandee].id))
            return panne(t);
        reglages::Zone relue; // la zone relue, pas celle qu'on croit avoir écrite
        const bool definie = hote_.zone_lire(relue);
        if (!definie && !source_seule)
            return panne(t);
        return corps_json(t, 200, [&](Ecrivain &e) {
            if (definie)
                ecrire_zone(e, relue);
            else
                ecrire_zone_absente(e);
        });
    }

    constexpr Reponse affichage_lire(const Tampons &t) noexcept {
        const reglages::Affichage a = hote_.affichage();
        return corps_json(t, 200, [&](Ecrivain &e) {
            e.ouvrir_objet();
            e.champ("langue_ecran", i18n::code(a.langue_ecran));
            e.champ("langue_assistant", i18n::code(a.langue_assistant));
            e.champ("theme", reglages::code(a.theme));
            e.champ_entier("luminosite", a.luminosite);
            for (std::string_view liste : {"langues", "langues_assistant"}) {
                e.cle(liste);
                e.ouvrir_tableau();
                for (std::size_t i = 0; i < i18n::NOMBRE_LANGUES; ++i) {
                    e.ouvrir_objet();
                    e.champ("code", i18n::CODES[i]);
                    e.champ("nom", i18n::NOMS[i]);
                    e.fermer_objet();
                }
                e.fermer_tableau();
            }
            e.cle("themes");
            e.ouvrir_tableau();
            for (std::string_view th : reglages::THEMES)
                e.chaine(th);
            e.fermer_tableau();
            e.fermer_objet();
        });
    }

    constexpr Reponse affichage_ecrire(const Requete &rq, const Tampons &t) noexcept {
        json::Valeur objet;
        Reponse refus;
        if (!lire_objet(rq, t, objet, refus))
            return refus;
        const reglages::Affichage avant = hote_.affichage();
        const reglages::ResultatAffichage v = reglages::valider_affichage(objet, avant);
        switch (v.erreur) {
        case reglages::ErreurAffichage::LangueEcran:
            return erreur(t, 422, "langue_ecran_inconnue", "Cette langue n’est pas disponible pour l’écran.",
                          "langue_ecran");
        case reglages::ErreurAffichage::LangueAssistant:
            return erreur(t, 422, "langue_assistant_inconnue", "Cette langue n’est pas disponible pour l’assistant.",
                          "langue_assistant");
        case reglages::ErreurAffichage::Theme:
            return erreur(t, 422, "theme_inconnu", "Ce thème n’existe pas.", "theme");
        case reglages::ErreurAffichage::Luminosite:
            return erreur(t, 422, "luminosite", "La luminosité doit être entre 5 et 100 %.", "luminosite");
        case reglages::ErreurAffichage::Aucune:
            break;
        }
        if (!(v.valeur == avant) && !hote_.enregistrer_affichage(v.valeur))
            return panne(t);
        return affichage_lire(t);
    }

    // Alertes officielles de Vigie (docs/api-reglages.md §14). `adresse` est vide tant que l'instance publique est utilisée.
    constexpr Reponse alertes_lire(const Tampons &t) noexcept {
        const reglages::Alertes a = hote_.alertes();
        return corps_json(t, 200, [&](Ecrivain &e) {
            e.ouvrir_objet();
            e.champ_booleen("actives", a.actives);
            e.champ("adresse", std::string_view(a.adresse));
            e.champ_entier("periode_min", a.periode_min);
            e.champ("niveau_min", reglages::code(a.niveau_min));
            e.champ("adresse_publique", vigie::ADRESSE_PUBLIQUE);
            e.cle("niveaux");
            e.ouvrir_tableau();
            for (std::string_view niveau : reglages::CODES_NIVEAU_MIN)
                e.chaine(niveau);
            e.fermer_tableau();
            e.fermer_objet();
        });
    }

    constexpr Reponse alertes_ecrire(const Requete &rq, const Tampons &t) noexcept {
        json::Valeur objet;
        Reponse refus;
        if (!lire_objet(rq, t, objet, refus))
            return refus;
        const reglages::Alertes avant = hote_.alertes();
        const reglages::ResultatAlertes v = reglages::valider_alertes(objet, avant);
        switch (v.erreur) {
        case reglages::ErreurAlertes::Actives:
            return erreur(t, 422, "actives_invalide", "Les alertes doivent être activées ou désactivées.", "actives");
        case reglages::ErreurAlertes::Adresse:
            return erreur(t, 422, "adresse_invalide", "L’adresse de Vigie doit commencer par https://.", "adresse");
        case reglages::ErreurAlertes::Periode:
            return erreur(t, 422, "periode_invalide", "La période doit être un nombre entier de minutes, de 10 à 180.",
                          "periode_min");
        case reglages::ErreurAlertes::NiveauMin:
            return erreur(t, 422, "niveau_inconnu", "Ce niveau d’alerte n’existe pas.", "niveau_min");
        case reglages::ErreurAlertes::Aucune:
            break;
        }
        if (!(v.valeur == avant) && !hote_.enregistrer_alertes(v.valeur))
            return panne(t);
        return alertes_lire(t);
    }

    constexpr Reponse meteo(const Tampons &t) noexcept {
        MeteoApercu m;
        if (!hote_.meteo_lire(m) || !m.valide) {
            return corps_json(t, 200, [](Ecrivain &e) {
                e.ouvrir_objet();
                e.champ_booleen("disponible", false);
                e.fermer_objet();
            });
        }
        const meteo::Condition c = meteo::decrire(m.code_wmo, m.jour);
        const std::string_view libelle = tr(c.libelle);
        return corps_json(t, 200, [&](Ecrivain &e) {
            e.ouvrir_objet();
            e.champ_booleen("disponible", true);
            e.champ_booleen("perimee", m.perimee);
            e.champ_entier("mise_a_jour", m.maj_epoch_s);
            // Qui a donné cette météo, à nommer partout où elle est montrée (licence, politesse). `repli` : la source choisie est un
            // service lu directement qui n'a pas répondu, la météo vient d'Open-Meteo en attendant (ADR-0014).
            namespace fm = meteo::fournisseurs;
            const fm::Attribution a = fm::attribution_de(m.origine);
            e.cle("fournisseur");
            e.ouvrir_objet();
            e.champ("id", a.id);
            e.champ("nom", a.nom);
            e.champ("lien", a.lien);
            e.fermer_objet();
            e.champ_booleen("repli", fm::famille_de(hote_.source_meteo()) != fm::Famille::OpenMeteo &&
                                         m.origine == fm::Famille::OpenMeteo);
            e.cle("actuelle");
            e.ouvrir_objet();
            e.champ_decimal("temperature", m.temperature_c, 1);
            e.champ_entier("code", m.code_wmo);
            e.champ("libelle", libelle);
            e.champ_booleen("jour", m.jour);
            e.fermer_objet();
            e.fermer_objet();
        });
    }

    constexpr Reponse geocoder(const Requete &rq, const Tampons &t) noexcept {
        char q[reglages::RECHERCHE_OCTETS_MAX + 1] = {};
        const reglages::Parametre p = reglages::parametre_decode(rq.requete, "q", q, sizeof q);
        if (p.invalide)
            return erreur(t, 400, "illisible", "Cette demande n’est pas lisible.", "q");
        if (p.tronque)
            return erreur(t, 400, "trop_long", "Cette recherche est trop longue.", "q");
        const reglages::ResultatRecherche v = reglages::valider_recherche(q);
        switch (v.erreur) {
        case reglages::ErreurRecherche::Illisible:
            return erreur(t, 400, "illisible", "Cette demande n’est pas lisible.", "q");
        case reglages::ErreurRecherche::TropCourte:
            return erreur(t, 400, "trop_court", "Tapez au moins 3 lettres.", "q");
        case reglages::ErreurRecherche::TropLongue:
            return erreur(t, 400, "trop_long", "Cette recherche est trop longue.", "q");
        case reglages::ErreurRecherche::Aucune:
            break;
        }
        char adresse[ADRESSE_GEOCODAGE_MAX] = {};
        Ecrivain a(adresse, sizeof adresse);
        reglages::adresse_geocodage(v.q, langue(), a);
        if (!a.ok())
            return panne(t);
        std::size_t recu = 0;
        const Telechargement d = hote_.telecharger(a.vue(), t.travail, t.cap_travail, recu);
        if (d != Telechargement::Ok || recu > t.cap_travail)
            return erreur(t, 503, "sans_internet", "L’appareil n’a pas accès à Internet pour chercher un lieu.");
        Ecrivain e(t.sortie, t.cap_sortie);
        if (!convertir_geocodage(std::string_view(t.travail, recu), e))
            return panne(t);
        Reponse r;
        r.statut = 200;
        r.corps = e.vue();
        return r;
    }

    constexpr Reponse wifi(const Tampons &t) noexcept {
        const InfoWifi w = hote_.wifi();
        const bool connecte = w.etat == EtatWifi::Connecte;
        std::string_view etat = "deconnecte";
        std::string_view modele = "Non connecté.";
        if (w.etat == EtatWifi::Connecte) {
            etat = "connecte";
            modele = "Connecté à « {ssid} ».";
        } else if (w.etat == EtatWifi::Connexion) {
            etat = "connexion";
            modele = "Connexion en cours…";
        } else if (w.etat == EtatWifi::Echec) {
            etat = "echec";
            modele = "La connexion a échoué.";
        }
        char message[200] = {};
        Ecrivain m(message, sizeof message);
        remplacer(tr(modele), "ssid", w.ssid, m);
        return corps_json(t, 200, [&](Ecrivain &e) {
            e.ouvrir_objet();
            e.champ("etat", etat);
            e.champ("ssid", w.ssid);
            e.champ_entier("niveau", connecte ? niveau_wifi(w.force_dbm) : 0);
            e.champ_entier("force", connecte ? w.force_dbm : 0);
            e.champ("ip", w.ip);
            e.champ("message", m.vue());
            e.fermer_objet();
        });
    }

    constexpr Reponse services(const Tampons &t) noexcept {
        return corps_json(t, 200, [](Ecrivain &e) {
            e.ouvrir_objet();
            for (std::string_view id : {"stt", "llm", "tts"}) {
                e.cle(id);
                e.ouvrir_objet();
                e.champ_booleen("configure", false);
                e.champ_booleen("cle_definie", false);
                e.fermer_objet();
            }
            e.fermer_objet();
        });
    }

    constexpr Reponse mise_a_jour(const Tampons &t) noexcept {
        const InfoSysteme s = hote_.systeme();
        return corps_json(t, 200, [&](Ecrivain &e) {
            e.ouvrir_objet();
            e.champ("etat", "inactif");
            e.champ_entier("progression", 0);
            e.champ("version", s.version);
            e.champ_booleen("validee", s.validee);
            e.fermer_objet();
        });
    }

    Hote &hote_;
    appairage::Gestionnaire appairage_{};
};

} // namespace ecran::api
