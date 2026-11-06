// Textes français de la phase 1b (heure, date, état du Wi-Fi) : PAS ENCORE dans la table de traduction.
//
// Pourquoi : la table (i18n/*.yaml, ADR-0006) se régénère avec web/app/lang/*.json et components/core/.../i18n_donnees.inc ; en 1b
// ni web/ ni le noyau pur ne sont touchés. test/host/controle_i18n.py ne lit que main/ui_bonjour.cpp et main/boot_info.cpp : ce
// fichier-ci lui échappe, donc il ne dit rien des textes d'ici. Ils sont listés dans docs/mesures/1b.md et à migrer vers la
// table (clé = texte français) en phase 2, puis ce fichier disparaît.
#pragma once

namespace ecran::texte_1b {

inline constexpr const char *kHeureAbsente = "-- h --";
inline constexpr const char *kDateAbsente = "Heure du réseau en attente";
inline constexpr const char *kWifiReglagesAbsents = "Wi-Fi : réglages absents du firmware";
inline constexpr const char *kWifiCoprocesseurAbsent = "Wi-Fi : coprocesseur absent";
inline constexpr const char *kWifiCoprocesseurDemarre = "Wi-Fi : coprocesseur en démarrage";
inline constexpr const char *kWifiConnexion = "Wi-Fi : connexion en cours";
inline constexpr const char *kWifiAdresse = "Wi-Fi : connecté, adresse en cours";
inline constexpr const char *kWifiConnecte = "Wi-Fi : connecté";
inline constexpr const char *kHeureEnAttente = "Heure : en attente du réseau";
inline constexpr const char *kHeureRecue = "Heure : reçue du réseau";

} // namespace ecran::texte_1b
