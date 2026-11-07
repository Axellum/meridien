// FICHIER GÉNÉRÉ par test/host/capturer_nvs.py : ne pas modifier à la main.
// Ce que l'analyse doit trouver dans les deux pages, calculé par Python (struct, zlib), pas par le noyau.
inline constexpr std::uint32_t ATTENDU_CLAIR_LISIBLES = 11;
inline constexpr std::uint32_t ATTENDU_CLAIR_ILLISIBLES = 0;
inline constexpr std::uint32_t ATTENDU_CHIFFRE_LISIBLES = 0;
inline constexpr std::uint32_t ATTENDU_CHIFFRE_ILLISIBLES = 15;
inline constexpr std::uint32_t ATTENDU_EFFACE_RESTES = 11;
inline constexpr std::uint32_t ATTENDU_CRC_123456789 = 0xd202d277;  // zlib.crc32(b"123456789", 0xFFFFFFFF)
