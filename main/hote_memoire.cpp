#include "hote_memoire.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>

namespace ecran {

namespace {

void copier(char *sortie, std::size_t capacite, std::string_view source) {
    if (capacite == 0) {
        return;
    }
    const std::size_t n = std::min(source.size(), capacite - 1);
    std::memcpy(sortie, source.data(), n);
    sortie[n] = '\0';
}

bool contient_sans_casse(const std::string &texte, std::string_view motif) {
    auto bas = [](unsigned char c) { return static_cast<char>((c >= 'A' && c <= 'Z') ? c + 32 : c); };
    for (std::size_t i = 0; i + motif.size() <= texte.size(); ++i) {
        std::size_t k = 0;
        while (k < motif.size() &&
               bas(static_cast<unsigned char>(texte[i + k])) == bas(static_cast<unsigned char>(motif[k]))) {
            ++k;
        }
        if (k == motif.size()) {
            return true;
        }
    }
    return false;
}

int rang_service(std::string_view service) {
    for (int i = 0; i < 3; ++i) {
        if (kServices[i] == service) {
            return i;
        }
    }
    return -1;
}

} // namespace

HoteMemoire::HoteMemoire() : valeurs_(registre::NOMBRE_REGLAGES) {
    remettre_par_defaut();
    std::snprintf(info.version, sizeof info.version, "%s", "0.1.0");
}

void HoteMemoire::remettre_par_defaut() {
    for (std::size_t i = 0; i < registre::NOMBRE_REGLAGES; ++i) {
        valeurs_[i].nombre = registre::REGISTRE[i].defaut;
        valeurs_[i].texte.clear();
    }
    progression_ = installation::Progression{};
    connexion_ = ConnexionWifi{};
    wifi_ = InfoWifi{};
    recherche_lieu_ = RechercheLieu::Inactive;
    trouves_.clear();
    meteo = ApercuMeteo{};
}

int HoteMemoire::index(std::string_view id) const {
    const registre::Reglage *r = registre::chercher(id);
    return r == nullptr ? -1 : static_cast<int>(r - registre::REGISTRE);
}

int64_t HoteMemoire::maintenant_ms() {
    if (horloge_fixee_ >= 0) {
        return horloge_fixee_;
    }
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// ---- réglages ------------------------------------------------------------------------------------------------------------

int32_t HoteMemoire::nombre(std::string_view id) {
    const int i = index(id);
    return i < 0 ? 0 : valeurs_[static_cast<std::size_t>(i)].nombre;
}

void HoteMemoire::definir_nombre(std::string_view id, int32_t valeur) {
    const int i = index(id);
    if (i < 0) {
        return;
    }
    const registre::Reglage &r = registre::REGISTRE[i];
    // La même règle que le registre : un nombre hors plage ou un choix hors liste est refusé, jamais enregistré.
    const bool valide = r.type == registre::Type::Nombre ? registre::nombre_valide(r, valeur)
                                                         : (r.type == registre::Type::Choix && valeur >= -1 &&
                                                            valeur < static_cast<int32_t>(r.nb_options));
    if (valide) {
        valeurs_[static_cast<std::size_t>(i)].nombre = valeur;
    }
}

std::size_t HoteMemoire::texte(std::string_view id, char *sortie, std::size_t capacite) {
    const int i = index(id);
    const registre::Reglage *r = i < 0 ? nullptr : &registre::REGISTRE[i];
    if (r == nullptr || r->type == registre::Type::Secret) { // un secret ne se relit jamais
        copier(sortie, capacite, {});
        return 0;
    }
    const std::string &t = valeurs_[static_cast<std::size_t>(i)].texte;
    copier(sortie, capacite, t);
    return capacite == 0 ? 0 : std::min(t.size(), capacite - 1);
}

void HoteMemoire::definir_texte(std::string_view id, std::string_view valeur) {
    const int i = index(id);
    if (i >= 0) {
        valeurs_[static_cast<std::size_t>(i)].texte.assign(valeur);
    }
}

bool HoteMemoire::secret_defini(std::string_view id) {
    const int i = index(id);
    return i >= 0 && !valeurs_[static_cast<std::size_t>(i)].texte.empty();
}

void HoteMemoire::effacer_secret(std::string_view id) {
    const int i = index(id);
    if (i >= 0) {
        valeurs_[static_cast<std::size_t>(i)].texte.clear();
    }
}

// ---- Wi-Fi ---------------------------------------------------------------------------------------------------------------

void HoteMemoire::wifi_chercher() { recherche_debut_ms_ = maintenant_ms(); }

bool HoteMemoire::wifi_recherche_en_cours() {
    return !recherche_instantanee && recherche_debut_ms_ >= 0 && maintenant_ms() - recherche_debut_ms_ < 800;
}

std::size_t HoteMemoire::wifi_reseaux(ReseauWifi *sortie, std::size_t max) {
    const std::size_t n = std::min(reseaux_vus.size(), max);
    for (std::size_t i = 0; i < n; ++i) {
        sortie[i] = reseaux_vus[i];
    }
    return n;
}

void HoteMemoire::wifi_connecter(std::string_view ssid, std::string_view mot_de_passe) {
    connexion_ = ConnexionWifi{wifi::Etat::Connexion, wifi::Cause::Aucune};
    connexion_debut_ms_ = maintenant_ms();
    connexion_ssid_.assign(ssid);
    connexion_mdp_.assign(mot_de_passe);
}

void HoteMemoire::wifi_abandonner() {
    connexion_ = ConnexionWifi{wifi::Etat::Echec, wifi::Cause::Delai};
    connexion_debut_ms_ = -1;
}

ConnexionWifi HoteMemoire::wifi_connexion() {
    if (connexion_.etat == wifi::Etat::Connexion && delai_connexion_ms >= 0 &&
        maintenant_ms() - connexion_debut_ms_ >= delai_connexion_ms) {
        bool protege = false;
        for (const ReseauWifi &r : reseaux_vus) {
            if (connexion_ssid_ == r.ssid) {
                protege = r.securise;
            }
        }
        if (!ssid_introuvable.empty() && connexion_ssid_ == ssid_introuvable) {
            connexion_ = ConnexionWifi{wifi::Etat::Echec, wifi::Cause::Introuvable};
        } else if (protege && connexion_mdp_ != mot_de_passe_attendu) {
            connexion_ = ConnexionWifi{wifi::Etat::Echec, wifi::Cause::MotDePasse};
        } else {
            connexion_ = ConnexionWifi{wifi::Etat::Reussi, wifi::Cause::Aucune};
            wifi_.connecte = true;
            copier(wifi_.ssid, sizeof wifi_.ssid, connexion_ssid_);
            copier(wifi_.ip, sizeof wifi_.ip, "192.168.1.52");
            definir_texte("wifi.ssid", connexion_ssid_); // le réseau est retenu pour le prochain démarrage
            definir_texte("wifi.mot_de_passe", connexion_mdp_);
        }
    }
    return connexion_;
}

InfoWifi HoteMemoire::wifi_info() { return wifi_; }

void HoteMemoire::simuler_connecte(std::string_view ssid, std::string_view ip) {
    wifi_.connecte = true;
    copier(wifi_.ssid, sizeof wifi_.ssid, ssid);
    copier(wifi_.ip, sizeof wifi_.ip, ip);
    definir_texte("wifi.ssid", ssid);
}

// ---- lieu ----------------------------------------------------------------------------------------------------------------

void HoteMemoire::lieu_chercher(std::string_view requete) {
    trouves_.clear();
    if (!internet) {
        recherche_lieu_ = RechercheLieu::SansConnexion;
        return;
    }
    for (const std::string &l : lieux_connus) {
        if (contient_sans_casse(l, requete)) {
            trouves_.push_back(l);
        }
    }
    recherche_lieu_ = RechercheLieu::Terminee;
}

RechercheLieu HoteMemoire::lieu_recherche() { return recherche_lieu_; }

std::size_t HoteMemoire::lieu_resultats(LieuTrouve *sortie, std::size_t max) {
    const std::size_t n = std::min({trouves_.size(), max, kLieuxMax});
    for (std::size_t i = 0; i < n; ++i) {
        copier(sortie[i].libelle, sizeof sortie[i].libelle, trouves_[i]);
    }
    return n;
}

void HoteMemoire::lieu_choisir(std::size_t rang) {
    if (rang < trouves_.size()) {
        definir_texte("lieu.ville", trouves_[rang]);
    }
}

ApercuMeteo HoteMemoire::meteo_apercu() { return meteo; }

// ---- appareil ------------------------------------------------------------------------------------------------------------

void HoteMemoire::tester_service(std::string_view service) {
    const int s = rang_service(service);
    if (s >= 0) {
        test_etat_[s] = EtatTest::EnCours;
        test_debut_ms_[s] = maintenant_ms();
    }
}

ResultatTest HoteMemoire::resultat_test(std::string_view service) {
    ResultatTest r;
    const int s = rang_service(service);
    if (s < 0) {
        return r;
    }
    if (test_etat_[s] == EtatTest::EnCours && maintenant_ms() - test_debut_ms_[s] >= delai_test_ms) {
        test_etat_[s] = EtatTest::Reussi;
    }
    r.etat = test_etat_[s];
    if (r.etat == EtatTest::Reussi) {
        copier(r.message, sizeof r.message, "Le service répond en 0,4 s.");
    }
    return r;
}

InfoAppareil HoteMemoire::info_appareil() { return info; }

void HoteMemoire::nouveau_code() {
    static int graine = 482913;
    graine = (graine * 7 + 123457) % 1000000;
    std::snprintf(info.code, sizeof info.code, "%06d", graine);
}

void HoteMemoire::redemarrer() { ++redemarrages; }

void HoteMemoire::reinitialiser() {
    ++reinitialisations;
    remettre_par_defaut();
    progression_ = installation::apres_reinitialisation();
}

} // namespace ecran
