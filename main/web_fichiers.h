// La page de réglages (web/app/) embarquée dans le firmware, compressée en gzip. Le contenu est généré à la compilation par
// tools/embarquer_web.py dans web_fichiers.cpp (dossier de construction) ; ce fichier-ci ne déclare que la forme de la table.
// Aucun en-tête ESP-IDF.
#pragma once

#include <cstddef>

namespace ecran {

struct FichierWeb {
    const char *chemin;        // « /index.html », « /lang/fr.json »
    const char *type;          // valeur de Content-Type
    const unsigned char *gzip; // octets compressés
    std::size_t taille_gzip;
    std::size_t taille_brute; // avant compression
    const char *etag;         // « "a1b2c3d4e5f60718" », guillemets compris
};

extern const FichierWeb kFichiersWeb[];
extern const unsigned kNombreFichiersWeb;
extern const unsigned kPoidsGzipWeb; // somme des tailles compressées
extern const unsigned kPoidsBrutWeb;

} // namespace ecran
