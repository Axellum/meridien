// UN SEUL TLS à la fois dans tout le firmware.
//
// Une poignée de main TLS demande ≈ 65 Ko de RAM interne (mesuré par jalon-1b le 06/11/2026) alors que la carte n'en a, au
// démarrage, qu'environ 110 Ko de libres : deux poignées de main en même temps (la météo et une recherche de ville, par exemple)
// épuisent la mémoire. Tout code qui ouvre une connexion HTTPS prend ce verrou AVANT `esp_http_client_init` et le rend APRÈS
// `esp_http_client_cleanup` (ou `esp_tls_conn_destroy`). Aujourd'hui un seul code le fait : le client HTTPS commun
// (main/http_commun.cpp), par lequel passent la météo, Vigie, la mer, la liste des spots et la recherche de ville.
//
// Mutex FreeRTOS non récursif : une tâche qui le tient ne doit pas le reprendre. Créé au premier appel (statique, sans allocation).
#pragma once

#include <cstdint>

namespace ecran {

// Vrai si le verrou est pris ; faux si un autre fil le tient encore après `delai_ms` millisecondes (ne jamais attendre sans fin).
bool verrou_tls_prendre(uint32_t delai_ms);
void verrou_tls_rendre();

// Prise du verrou limitée à une portée : rendu à la sortie, quel que soit le chemin (`return` anticipé compris).
class VerrouTls {
  public:
    explicit VerrouTls(uint32_t delai_ms) : pris_(verrou_tls_prendre(delai_ms)) {}
    ~VerrouTls() {
        if (pris_) {
            verrou_tls_rendre();
        }
    }
    VerrouTls(const VerrouTls &) = delete;
    VerrouTls &operator=(const VerrouTls &) = delete;

    bool pris() const { return pris_; }

  private:
    bool pris_;
};

} // namespace ecran
