// Noyau pur : écriture JSON dans un tampon de taille fixe, sans allocation, sans printf, sans exception.
// Sert à la page de réglages (docs/api-reglages.md) : les réponses de l'API sont écrites ici, pas par concaténation à la main.
//
// Garanties : jamais d'écriture hors du tampon (un débordement met `ok()` à faux et le reste est ignoré : l'appelant refuse la
// réponse au lieu d'en envoyer une tronquée) ; une chaîne est toujours échappée (guillemet, antislash, caractères de
// contrôle) ; les nombres décimaux sont écrits par arithmétique entière (pas de `%f` sur l'appareil). UTF-8 passé tel quel
// (accents et « ° » restent lisibles, api-reglages.md §1 règle 3). Évaluable à la compilation (les tests de test/host/ s'en
// servent). C++17 standard, aucun include ESP-IDF.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace ecran::api {

class Ecrivain {
  public:
    constexpr Ecrivain(char *tampon, std::size_t capacite) noexcept : tampon_(tampon), capacite_(capacite) {}

    constexpr bool ok() const noexcept { return ok_; }
    constexpr std::size_t taille() const noexcept { return n_; }
    constexpr std::string_view vue() const noexcept { return std::string_view(tampon_, n_); }

    // --- octets bruts ---
    constexpr void octet(char c) noexcept {
        if (n_ < capacite_) {
            tampon_[n_++] = c;
        } else {
            ok_ = false;
        }
    }
    constexpr void brut(std::string_view s) noexcept {
        for (char c : s)
            octet(c);
    }

    // --- structure ---
    constexpr void ouvrir_objet() noexcept { ouvrir('{'); }
    constexpr void fermer_objet() noexcept { fermer('}'); }
    constexpr void ouvrir_tableau() noexcept { ouvrir('['); }
    constexpr void fermer_tableau() noexcept { fermer(']'); }

    // Nom d'un membre d'objet ; la valeur qui suit n'écrit pas de virgule devant elle.
    constexpr void cle(std::string_view nom) noexcept {
        separer();
        texte_echappe(nom);
        octet(':');
        apres_cle_ = true;
    }

    // --- valeurs ---
    constexpr void chaine(std::string_view s) noexcept {
        avant_valeur();
        texte_echappe(s);
        apres_valeur();
    }
    constexpr void entier(long long v) noexcept {
        avant_valeur();
        chiffres(v);
        apres_valeur();
    }
    constexpr void booleen(bool b) noexcept {
        avant_valeur();
        brut(b ? "true" : "false");
        apres_valeur();
    }
    constexpr void nul() noexcept {
        avant_valeur();
        brut("null");
        apres_valeur();
    }
    // Nombre décimal avec au plus `decimales` chiffres après la virgule (0 à 9), zéros de fin retirés : 14.2, 45.74846, 4.
    // Un nombre non fini ou de plus de 1e9 en valeur absolue est écrit `null` (jamais une valeur inventée).
    constexpr void decimal(double v, int decimales) noexcept {
        avant_valeur();
        if (!(v == v) || v > 1e9 || v < -1e9 || decimales < 0 || decimales > 9) {
            brut("null");
        } else {
            const bool negatif = v < 0;
            const double a = negatif ? -v : v;
            std::uint64_t echelle = 1;
            for (int i = 0; i < decimales; ++i)
                echelle *= 10;
            const std::uint64_t s = static_cast<std::uint64_t>(a * static_cast<double>(echelle) + 0.5);
            if (negatif && s != 0)
                octet('-');
            chiffres(static_cast<long long>(s / echelle));
            std::uint64_t frac = s % echelle;
            if (frac != 0) {
                octet('.');
                int restant = decimales;
                std::uint64_t diviseur = echelle / 10;
                while (restant > 0 && frac != 0) {
                    octet(static_cast<char>('0' + frac / diviseur));
                    frac %= diviseur;
                    diviseur = diviseur > 1 ? diviseur / 10 : 1;
                    --restant;
                }
            }
        }
        apres_valeur();
    }

    // --- raccourcis « membre : valeur » ---
    constexpr void champ(std::string_view nom, std::string_view valeur) noexcept {
        cle(nom);
        chaine(valeur);
    }
    constexpr void champ_entier(std::string_view nom, long long valeur) noexcept {
        cle(nom);
        entier(valeur);
    }
    constexpr void champ_booleen(std::string_view nom, bool valeur) noexcept {
        cle(nom);
        booleen(valeur);
    }
    constexpr void champ_decimal(std::string_view nom, double valeur, int decimales) noexcept {
        cle(nom);
        decimal(valeur, decimales);
    }

  private:
    constexpr void separer() noexcept {
        if (virgule_)
            octet(',');
        virgule_ = false;
    }
    constexpr void avant_valeur() noexcept {
        if (!apres_cle_)
            separer();
        apres_cle_ = false;
    }
    constexpr void apres_valeur() noexcept { virgule_ = true; }
    constexpr void ouvrir(char c) noexcept {
        avant_valeur();
        octet(c);
        virgule_ = false;
    }
    constexpr void fermer(char c) noexcept {
        octet(c);
        virgule_ = true;
    }

    constexpr void chiffres(long long v) noexcept {
        std::uint64_t m = v < 0 ? (~static_cast<std::uint64_t>(v) + 1) : static_cast<std::uint64_t>(v);
        if (v < 0)
            octet('-');
        char renverse[20] = {};
        int n = 0;
        do {
            renverse[n++] = static_cast<char>('0' + m % 10);
            m /= 10;
        } while (m != 0);
        while (n > 0)
            octet(renverse[--n]);
    }

    constexpr void texte_echappe(std::string_view s) noexcept {
        constexpr char HEX[] = "0123456789abcdef";
        octet('"');
        for (char ch : s) {
            const unsigned char c = static_cast<unsigned char>(ch);
            if (c == '"') {
                brut("\\\"");
            } else if (c == '\\') {
                brut("\\\\");
            } else if (c == '\n') {
                brut("\\n");
            } else if (c == '\r') {
                brut("\\r");
            } else if (c == '\t') {
                brut("\\t");
            } else if (c < 0x20) {
                brut("\\u00");
                octet(HEX[c >> 4]);
                octet(HEX[c & 15]);
            } else {
                octet(ch);
            }
        }
        octet('"');
    }

    char *tampon_;
    std::size_t capacite_;
    std::size_t n_ = 0;
    bool ok_ = true;
    bool virgule_ = false;
    bool apres_cle_ = false;
};

// Entier en texte décimal, dans `tampon` (pour un identifiant écrit comme une chaîne, ou inséré dans une phrase).
constexpr std::string_view entier_en_texte(std::int64_t v, char (&tampon)[24]) noexcept {
    Ecrivain e(tampon, sizeof tampon);
    e.brut("");
    std::uint64_t m = v < 0 ? (~static_cast<std::uint64_t>(v) + 1) : static_cast<std::uint64_t>(v);
    char renverse[24] = {};
    int n = 0;
    do {
        renverse[n++] = static_cast<char>('0' + m % 10);
        m /= 10;
    } while (m != 0);
    if (v < 0)
        e.octet('-');
    while (n > 0)
        e.octet(renverse[--n]);
    return e.vue();
}

} // namespace ecran::api
