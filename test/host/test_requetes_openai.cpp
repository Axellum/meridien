// Tests du noyau pur : requêtes vers un service compatible OpenAI (requetes_openai.hpp).
#include "ecran/assistant_texte.hpp"
#include "ecran/requetes_openai.hpp"
#include "mini.hpp"

using namespace ecran::assistant;

namespace {

constexpr bool egal(std::string_view a, std::string_view b) { return a == b; }

template <std::size_t N>
struct Sortie {
    char t[N] = {};
    Tampon tp{t, N};
    constexpr std::string_view vue() const { return tp.vue(); }
};

// Frontière de référence : ecran- + 16 chiffres hexadécimaux de 0x0123456789abcdef.
#define B "ecran-0123456789abcdef"
constexpr std::string_view FRONTIERE = B;

// --- adresse -----------------------------------------------------------------------------------------------

constexpr int assemblage_adresse_et_route() {
    struct Cas {
        std::string_view base, route, attendu;
    };
    constexpr Cas CAS_URL[] = {
        {"https://api.openai.com/v1", "chat/completions", "https://api.openai.com/v1/chat/completions"},
        {"https://api.openai.com/v1/", "/chat/completions", "https://api.openai.com/v1/chat/completions"},
        {"https://api.openai.com/v1///", "///audio/speech", "https://api.openai.com/v1/audio/speech"},
        {"http://192.168.0.20:1234/v1", "audio/transcriptions", "http://192.168.0.20:1234/v1/audio/transcriptions"},
        // « /v1 » doublé : la base finit par /v1 et la route commence par v1/
        {"http://192.168.0.20:1234/v1", "v1/chat/completions", "http://192.168.0.20:1234/v1/chat/completions"},
        {"http://192.168.0.20:1234/v1/", "/v1/audio/speech", "http://192.168.0.20:1234/v1/audio/speech"},
        {"http://h/api/v1", "v1/models", "http://h/api/v1/models"},
        {"http://h/v1", "v1", "http://h/v1"},
        // pas un vrai doublon : on ne retire que le segment « v1 » entier
        {"http://h/v10", "v1/models", "http://h/v10/v1/models"},
        {"http://h/v1", "v1x/abc", "http://h/v1/v1x/abc"},
        {"http://h/v1", "v2/abc", "http://h/v1/v2/abc"},
        // « /v1 » manquant : NON corrigé en silence (c'est l'utilisateur qui le voit dans le message d'erreur)
        {"http://192.168.0.20:1234", "chat/completions", "http://192.168.0.20:1234/chat/completions"},
        {"http://192.168.0.20:1234/", "chat/completions", "http://192.168.0.20:1234/chat/completions"},
        // Google : « …/v1beta/openai » n'a pas de /v1 et n'en reçoit pas
        {"https://generativelanguage.googleapis.com/v1beta/openai/", "chat/completions",
         "https://generativelanguage.googleapis.com/v1beta/openai/chat/completions"},
        {"http://h", "", "http://h"},
    };
    for (const Cas& c : CAS_URL) {
        Sortie<256> s;
        VERIFIER(assembler_url(s.tp, c.base, c.route));
        VERIFIER(egal(s.vue(), c.attendu));
    }
    return 0;
}

constexpr int adresses_refusees() {
    for (std::string_view base : {std::string_view(""), std::string_view("ftp://h/v1"), std::string_view("h/v1"),
                                  std::string_view("//h/v1"), std::string_view("localhost:1234/v1"),
                                  std::string_view("http://h/ v1"), std::string_view("http://h/v1?api-version=1"),
                                  std::string_view("http://h/v1#x"), std::string_view("http://h\n/v1"),
                                  std::string_view("https://h/v1\t")}) {
        Sortie<256> s;
        VERIFIER(!assembler_url(s.tp, base, "chat/completions"));
    }
    Sortie<256> s;
    VERIFIER(!assembler_url(s.tp, "http://h/v1", "chat completions"));  // espace dans la route
    return 0;
}

constexpr int adresse_trop_longue_pour_le_tampon_deborde_sans_rien_couper_en_silence() {
    Sortie<20> s;
    VERIFIER(!assembler_url(s.tp, "https://api.openai.com/v1", "chat/completions"));
    VERIFIER(s.tp.deborde());
    Sortie<41> pile;  // « https://api.openai.com/v1/chat/completions » = 42 octets : un de trop
    VERIFIER(!assembler_url(pile.tp, "https://api.openai.com/v1", "chat/completions"));
    Sortie<42> ok;
    VERIFIER(assembler_url(ok.tp, "https://api.openai.com/v1", "chat/completions"));
    return 0;
}

constexpr int la_base_a_t_elle_une_version() {
    VERIFIER(base_a_une_version("https://api.openai.com/v1"));
    VERIFIER(base_a_une_version("https://api.openai.com/v1/"));
    VERIFIER(base_a_une_version("http://pc:1234/v1"));
    VERIFIER(base_a_une_version("http://h/v1beta/openai"));
    VERIFIER(base_a_une_version("http://h/api/v2alpha1"));
    VERIFIER(base_a_une_version("http://h/api/v2/openai"));
    VERIFIER(!base_a_une_version("http://pc:1234"));
    VERIFIER(!base_a_une_version("http://pc:1234/"));
    VERIFIER(!base_a_une_version("http://h/v"));
    VERIFIER(!base_a_une_version("http://h/version"));
    VERIFIER(!base_a_une_version("http://h/vbeta"));
    VERIFIER(!base_a_une_version("http://h/1"));
    VERIFIER(!base_a_une_version("http://h/V1"));
    VERIFIER(!base_a_une_version("http://h/v1.5"));
    VERIFIER(!base_a_une_version("pc:1234/v1"));  // pas d'adresse complète
    VERIFIER(!base_a_une_version("http://v1"));   // « v1 » est l'hôte, pas un segment du chemin
    return 0;
}

constexpr int routes_documentees() {
    VERIFIER(egal(ROUTE_CHAT, "chat/completions"));
    VERIFIER(egal(ROUTE_TTS, "audio/speech"));
    VERIFIER(egal(ROUTE_STT, "audio/transcriptions"));
    VERIFIER(egal(ROUTE_MODELES, "models"));
    return 0;
}

// --- échappement JSON --------------------------------------------------------------------------------------

constexpr int json_texte_simple_et_caracteres_speciaux() {
    Sortie<128> a;
    VERIFIER(json_chaine(a.tp, "abc 123 /"));
    VERIFIER(egal(a.vue(), "\"abc 123 /\""));  // la barre oblique n'est pas échappée
    Sortie<128> b;
    VERIFIER(json_chaine(b.tp, "a\"b\\c\nd\re\tf\bg\fh"));
    VERIFIER(egal(b.vue(), "\"a\\\"b\\\\c\\nd\\re\\tf\\bg\\fh\""));
    Sortie<16> vide;
    VERIFIER(json_chaine(vide.tp, ""));
    VERIFIER(egal(vide.vue(), "\"\""));
    return 0;
}

constexpr int json_caracteres_de_controle_en_u00xx() {
    constexpr char CTRL[] = {'\x01', '\x1f', '\x00', '\x0b', 'x', '\x7f'};
    Sortie<128> s;
    VERIFIER(json_chaine(s.tp, std::string_view(CTRL, sizeof(CTRL))));
    VERIFIER(egal(s.vue(), "\"\\u0001\\u001f\\u0000\\u000bx\x7f\""));  // DEL est valide tel quel en JSON
    for (int c = 0; c < 0x20; ++c) {  // aucun octet < 0x20 ne reste brut dans la sortie
        const char un = static_cast<char>(c);
        Sortie<32> t;
        VERIFIER(json_chaine(t.tp, std::string_view(&un, 1)));
        for (char o : t.vue()) VERIFIER(static_cast<unsigned char>(o) >= 0x20U);
    }
    return 0;
}

constexpr int json_accents_euro_emoji_restent_en_utf8() {
    Sortie<128> s;
    constexpr std::string_view texte = "Ça va ? « Élève » 5 € 😀 °C";
    VERIFIER(json_chaine(s.tp, texte));
    VERIFIER(s.vue().size() == texte.size() + 2);
    VERIFIER(s.vue().substr(1, texte.size()) == texte);
    VERIFIER(utf8::valide(s.vue()));
    return 0;
}

constexpr int json_octets_invalides_remplaces_par_u_fffd() {
    constexpr std::string_view F = "\xEF\xBF\xBD";
    struct Cas {
        std::string_view entree;
        std::string_view attendu;
    };
    constexpr Cas CAS_J[] = {
        {"\xFF", "\"\xEF\xBF\xBD\""},
        {"a\xC3", "\"a\xEF\xBF\xBD\""},                                   // é coupé : octet de tête seul
        {"\xC3(", "\"\xEF\xBF\xBD(\""},                                    // continuation attendue, « ( » reçu
        {"\xE2\x82", "\"\xEF\xBF\xBD\xEF\xBF\xBD\""},                      // € coupé : un U+FFFD par octet
        {"\xC0\x80", "\"\xEF\xBF\xBD\xEF\xBF\xBD\""},                      // forme sur-longue de NUL
        {"\xED\xA0\x80", "\"\xEF\xBF\xBD\xEF\xBF\xBD\xEF\xBF\xBD\""},      // surrogate U+D800 encodé
        {"\xF4\x90\x80\x80", "\"\xEF\xBF\xBD\xEF\xBF\xBD\xEF\xBF\xBD\xEF\xBF\xBD\""},  // > U+10FFFF
        {"\x80", "\"\xEF\xBF\xBD\""},                                      // continuation isolée
        {"é\xFF" "é", "\"é\xEF\xBF\xBD" "é\""},                          // le texte valide autour est conservé
    };
    VERIFIER(F.size() == 3);
    for (const Cas& c : CAS_J) {
        Sortie<64> s;
        VERIFIER(json_chaine(s.tp, c.entree));
        VERIFIER(egal(s.vue(), c.attendu));
        VERIFIER(utf8::valide(s.vue()));
    }
    return 0;
}

constexpr int json_debordement_est_signale() {
    Sortie<5> s;
    VERIFIER(!json_chaine(s.tp, "abcdef"));
    VERIFIER(s.tp.deborde());
    Sortie<8> pile;  // "abcdef" = 8 octets avec les guillemets
    VERIFIER(json_chaine(pile.tp, "abcdef"));
    Sortie<7> trop_court;
    VERIFIER(!json_chaine(trop_court.tp, "abcdef"));
    return 0;
}

// --- chat --------------------------------------------------------------------------------------------------

constexpr int corps_chat_exact_avec_modele_et_prompt() {
    constexpr Message H[] = {{Role::Utilisateur, "Bonjour"}};
    OptionsChat o;
    o.modele = "m";
    o.prompt_systeme = "Sois bref.";
    o.historique = H;
    o.nombre = 1;
    Sortie<512> s;
    VERIFIER(corps_chat(s.tp, o));
    VERIFIER(egal(s.vue(),
                  "{\"model\":\"m\",\"stream\":true,\"messages\":[{\"role\":\"system\",\"content\":\"Sois bref.\"},"
                  "{\"role\":\"user\",\"content\":\"Bonjour\"}]}"));
    return 0;
}

constexpr int corps_chat_sans_modele_ni_prompt() {
    constexpr Message H[] = {{Role::Utilisateur, "Salut"}};
    OptionsChat o;
    o.historique = H;
    o.nombre = 1;
    Sortie<512> s;
    VERIFIER(corps_chat(s.tp, o));
    VERIFIER(egal(s.vue(), "{\"stream\":true,\"messages\":[{\"role\":\"user\",\"content\":\"Salut\"}]}"));
    return 0;
}

constexpr int corps_chat_echappe_les_contenus() {
    constexpr Message H[] = {{Role::Utilisateur, "Dis \"oui\"\nà la ligne"}, {Role::Assistant, "Ça\tmarche"}};
    OptionsChat o;
    o.modele = "m\"x";
    o.prompt_systeme = "a\\b";
    o.historique = H;
    o.nombre = 2;
    Sortie<512> s;
    VERIFIER(corps_chat(s.tp, o));
    VERIFIER(egal(s.vue(),
                  "{\"model\":\"m\\\"x\",\"stream\":true,\"messages\":[{\"role\":\"system\",\"content\":\"a\\\\b\"},"
                  "{\"role\":\"user\",\"content\":\"Dis \\\"oui\\\"\\nà la ligne\"},"
                  "{\"role\":\"assistant\",\"content\":\"Ça\\tmarche\"}]}"));
    return 0;
}

constexpr int corps_chat_refuse_l_absence_de_message_et_le_debordement() {
    OptionsChat o;
    Sortie<512> s;
    VERIFIER(!corps_chat(s.tp, o));  // aucun message
    constexpr Message H[] = {{Role::Utilisateur, "Bonjour"}};
    o.historique = H;
    o.nombre = 1;
    Sortie<30> petit;
    VERIFIER(!corps_chat(petit.tp, o) && petit.tp.deborde());
    return 0;
}

constexpr int fenetre_d_historique_bornee_en_octets() {
    constexpr Message M[] = {{Role::Utilisateur, "aaaa"}, {Role::Assistant, "bbbb"}, {Role::Utilisateur, "cccc"},
                             {Role::Assistant, "dddd"},  {Role::Utilisateur, "eeee"}};
    VERIFIER(premier_message_retenu(M, 5, 100) == 0);
    VERIFIER(premier_message_retenu(M, 5, 20) == 0);  // pile le budget : tout tient
    VERIFIER(premier_message_retenu(M, 5, 19) == 2);  // « bbbb » sort, et la réponse orpheline avec la question « aaaa »
    VERIFIER(premier_message_retenu(M, 5, 16) == 2);
    VERIFIER(premier_message_retenu(M, 5, 12) == 2);  // c, d, e = 12 octets
    VERIFIER(premier_message_retenu(M, 5, 11) == 4);  // « cccc » ne tient plus : reste « eeee » (d orphelin écarté)
    VERIFIER(premier_message_retenu(M, 5, 8) == 4);
    VERIFIER(premier_message_retenu(M, 5, 4) == 4);
    VERIFIER(premier_message_retenu(M, 5, 0) == 4);   // la dernière question est toujours envoyée
    VERIFIER(premier_message_retenu(M, 1, 0) == 0);
    VERIFIER(premier_message_retenu(M, 0, 100) == 0);
    VERIFIER(premier_message_retenu(nullptr, 3, 100) == 0);
    constexpr Message GROS[] = {{Role::Utilisateur, "a"}, {Role::Utilisateur, "zzzzzzzzzz"}};
    VERIFIER(premier_message_retenu(GROS, 2, 5) == 1);  // un dernier message plus gros que le budget reste seul
    return 0;
}

constexpr int le_corps_ne_contient_que_la_fenetre() {
    constexpr Message M[] = {{Role::Utilisateur, "aaaa"}, {Role::Assistant, "bbbb"}, {Role::Utilisateur, "cccc"},
                             {Role::Assistant, "dddd"},  {Role::Utilisateur, "eeee"}};
    OptionsChat o;
    o.prompt_systeme = "S";
    o.historique = M;
    o.nombre = 5;
    o.budget_octets = 12;
    Sortie<512> s;
    VERIFIER(corps_chat(s.tp, o));
    VERIFIER(egal(s.vue(),
                  "{\"stream\":true,\"messages\":[{\"role\":\"system\",\"content\":\"S\"},"
                  "{\"role\":\"user\",\"content\":\"cccc\"},{\"role\":\"assistant\",\"content\":\"dddd\"},"
                  "{\"role\":\"user\",\"content\":\"eeee\"}]}"));
    return 0;
}

constexpr int le_prompt_systeme_ne_compte_pas_dans_le_budget() {
    constexpr Message M[] = {{Role::Utilisateur, "aaaa"}};
    OptionsChat o;
    o.prompt_systeme = "un prompt systeme bien plus long que le budget";
    o.historique = M;
    o.nombre = 1;
    o.budget_octets = 4;
    Sortie<512> s;
    VERIFIER(corps_chat(s.tp, o));
    VERIFIER(s.vue().find("aaaa") != std::string_view::npos);
    return 0;
}

constexpr int noms_des_roles() {
    VERIFIER(egal(nom_role(Role::Systeme), "system"));
    VERIFIER(egal(nom_role(Role::Utilisateur), "user"));
    VERIFIER(egal(nom_role(Role::Assistant), "assistant"));
    return 0;
}

// --- TTS ---------------------------------------------------------------------------------------------------

constexpr int corps_tts_exact() {
    OptionsTts o;
    o.modele = "m";
    o.voix = "v";
    o.texte = "Bonjour.";
    Sortie<256> s;
    VERIFIER(corps_tts(s.tp, o));
    VERIFIER(egal(s.vue(), "{\"model\":\"m\",\"input\":\"Bonjour.\",\"voice\":\"v\",\"response_format\":\"pcm\"}"));
    return 0;
}

constexpr int corps_tts_omet_les_champs_vides_et_choisit_le_format() {
    OptionsTts o;
    o.texte = "x";
    o.format = FormatAudio::Wav;
    Sortie<256> s;
    VERIFIER(corps_tts(s.tp, o));
    VERIFIER(egal(s.vue(), "{\"input\":\"x\",\"response_format\":\"wav\"}"));
    VERIFIER(egal(nom_format_audio(FormatAudio::Pcm), "pcm"));
    VERIFIER(egal(nom_format_audio(FormatAudio::Mp3), "mp3"));
    VERIFIER(egal(nom_format_audio(FormatAudio::Opus), "opus"));
    return 0;
}

constexpr int corps_tts_echappe_le_texte_a_dire() {
    OptionsTts o;
    o.texte = "Ça va ? « Oui », dit-il.\nFin \"ok\" \\ 5 €";
    Sortie<256> s;
    VERIFIER(corps_tts(s.tp, o));
    VERIFIER(egal(s.vue(), "{\"input\":\"Ça va ? « Oui », dit-il.\\nFin \\\"ok\\\" \\\\ 5 €\",\"response_format\":\"pcm\"}"));
    return 0;
}

constexpr int corps_tts_refuse_texte_vide_et_debordement() {
    OptionsTts o;
    Sortie<256> s;
    VERIFIER(!corps_tts(s.tp, o));
    o.texte = "Bonjour";
    Sortie<20> petit;
    VERIFIER(!corps_tts(petit.tp, o));
    return 0;
}

// --- multipart (STT) ---------------------------------------------------------------------------------------

constexpr int frontiere_depuis_une_graine() {
    Sortie<64> s;
    VERIFIER(ecrire_frontiere(s.tp, 0x0123456789abcdefULL));
    VERIFIER(egal(s.vue(), FRONTIERE));
    VERIFIER(frontiere_valide(s.vue()));
    Sortie<64> z;
    VERIFIER(ecrire_frontiere(z.tp, 0));
    VERIFIER(egal(z.vue(), "ecran-0000000000000000"));
    Sortie<64> m;
    VERIFIER(ecrire_frontiere(m.tp, 0xFFFFFFFFFFFFFFFFULL));
    VERIFIER(egal(m.vue(), "ecran-ffffffffffffffff"));
    Sortie<10> petit;
    VERIFIER(!ecrire_frontiere(petit.tp, 1));
    return 0;
}

constexpr int frontieres_admissibles_et_refusees() {
    VERIFIER(frontiere_valide("abc-DEF_123"));
    VERIFIER(frontiere_valide("a"));
    VERIFIER(!frontiere_valide(""));
    VERIFIER(!frontiere_valide("a b"));
    VERIFIER(!frontiere_valide("a\r\nb"));
    VERIFIER(!frontiere_valide("a\"b"));
    VERIFIER(!frontiere_valide("é"));
    constexpr char SOIXANTE_DIX[] = "0123456789012345678901234567890123456789012345678901234567890123456789";
    VERIFIER(frontiere_valide(std::string_view(SOIXANTE_DIX, 70)));
    constexpr char SOIXANTE_ET_ONZE[] = "01234567890123456789012345678901234567890123456789012345678901234567890";
    VERIFIER(!frontiere_valide(std::string_view(SOIXANTE_ET_ONZE, 71)));
    return 0;
}

constexpr int en_tete_content_type() {
    Sortie<128> s;
    VERIFIER(type_contenu_multipart(s.tp, FRONTIERE));
    VERIFIER(egal(s.vue(), "multipart/form-data; boundary=" B));
    Sortie<128> mauvais;
    VERIFIER(!type_contenu_multipart(mauvais.tp, "a b"));
    return 0;
}

constexpr int cadre_multipart_exact_avant_et_apres_l_audio() {
    OptionsStt o;
    o.modele = "m";
    o.langue = "fr";
    Sortie<512> d;
    VERIFIER(debut_multipart(d.tp, FRONTIERE, o));
    VERIFIER(egal(d.vue(),
                  "--" B "\r\nContent-Disposition: form-data; name=\"model\"\r\n\r\nm\r\n"
                  "--" B "\r\nContent-Disposition: form-data; name=\"language\"\r\n\r\nfr\r\n"
                  "--" B "\r\nContent-Disposition: form-data; name=\"response_format\"\r\n\r\njson\r\n"
                  "--" B "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n"
                  "Content-Type: audio/wav\r\n\r\n"));
    Sortie<64> f;
    VERIFIER(fin_multipart(f.tp, FRONTIERE));
    VERIFIER(egal(f.vue(), "\r\n--" B "--\r\n"));
    return 0;
}

constexpr int cadre_multipart_omet_modele_et_langue_vides() {
    OptionsStt o;
    o.nom_fichier = "voix.mp3";
    o.type_mime = "audio/mpeg";
    Sortie<512> d;
    VERIFIER(debut_multipart(d.tp, FRONTIERE, o));
    VERIFIER(egal(d.vue(),
                  "--" B "\r\nContent-Disposition: form-data; name=\"response_format\"\r\n\r\njson\r\n"
                  "--" B "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"voix.mp3\"\r\n"
                  "Content-Type: audio/mpeg\r\n\r\n"));
    return 0;
}

constexpr int multipart_refuse_les_valeurs_dangereuses() {
    Sortie<512> s;
    OptionsStt o;
    o.modele = "m\r\n--" B "--";  // tente d'imiter la frontière
    VERIFIER(!debut_multipart(s.tp, FRONTIERE, o));
    o.modele = "m\"x";
    VERIFIER(!debut_multipart(s.tp, FRONTIERE, o));
    o.modele = "m\ny";
    VERIFIER(!debut_multipart(s.tp, FRONTIERE, o));
    o.modele = "contient-" B "-dedans";
    VERIFIER(!debut_multipart(s.tp, FRONTIERE, o));
    o.modele = "ok";
    o.langue = "f\rr";
    VERIFIER(!debut_multipart(s.tp, FRONTIERE, o));
    o.langue = "fr";
    o.nom_fichier = "a\".wav";
    VERIFIER(!debut_multipart(s.tp, FRONTIERE, o));
    o.nom_fichier = "../x.wav";
    VERIFIER(!debut_multipart(s.tp, FRONTIERE, o));  // pas de « / » dans un nom de fichier
    VERIFIER(s.tp.taille() == 0);                     // et rien n'a été écrit
    return 0;
}

constexpr int multipart_refuse_nom_de_fichier_type_et_frontiere_invalides() {
    OptionsStt o;
    o.nom_fichier = "a/b.wav";
    Sortie<512> a;
    VERIFIER(!debut_multipart(a.tp, FRONTIERE, o));
    o.nom_fichier = "";
    Sortie<512> b;
    VERIFIER(!debut_multipart(b.tp, FRONTIERE, o));
    o.nom_fichier = "a.wav";
    o.type_mime = "audio/wav; x=y";
    Sortie<512> c;
    VERIFIER(!debut_multipart(c.tp, FRONTIERE, o));
    o.type_mime = "audio/wav";
    Sortie<512> d;
    VERIFIER(!debut_multipart(d.tp, "mauvaise frontiere", o));
    Sortie<512> e;
    VERIFIER(!fin_multipart(e.tp, ""));
    Sortie<40> petit;
    VERIFIER(!debut_multipart(petit.tp, FRONTIERE, o) && petit.tp.deborde());
    return 0;
}

}  // namespace

#define LISTE(X)                                                                         \
    X(assemblage_adresse_et_route)                                                       \
    X(adresses_refusees)                                                                 \
    X(adresse_trop_longue_pour_le_tampon_deborde_sans_rien_couper_en_silence)            \
    X(la_base_a_t_elle_une_version)                                                      \
    X(routes_documentees)                                                                \
    X(json_texte_simple_et_caracteres_speciaux)                                          \
    X(json_caracteres_de_controle_en_u00xx)                                              \
    X(json_accents_euro_emoji_restent_en_utf8)                                           \
    X(json_octets_invalides_remplaces_par_u_fffd)                                        \
    X(json_debordement_est_signale)                                                      \
    X(corps_chat_exact_avec_modele_et_prompt)                                            \
    X(corps_chat_sans_modele_ni_prompt)                                                  \
    X(corps_chat_echappe_les_contenus)                                                   \
    X(corps_chat_refuse_l_absence_de_message_et_le_debordement)                          \
    X(fenetre_d_historique_bornee_en_octets)                                             \
    X(le_corps_ne_contient_que_la_fenetre)                                               \
    X(le_prompt_systeme_ne_compte_pas_dans_le_budget)                                    \
    X(noms_des_roles)                                                                    \
    X(corps_tts_exact)                                                                   \
    X(corps_tts_omet_les_champs_vides_et_choisit_le_format)                              \
    X(corps_tts_echappe_le_texte_a_dire)                                                 \
    X(corps_tts_refuse_texte_vide_et_debordement)                                        \
    X(frontiere_depuis_une_graine)                                                       \
    X(frontieres_admissibles_et_refusees)                                                \
    X(en_tete_content_type)                                                              \
    X(cadre_multipart_exact_avant_et_apres_l_audio)                                      \
    X(cadre_multipart_omet_modele_et_langue_vides)                                       \
    X(multipart_refuse_les_valeurs_dangereuses)                                          \
    X(multipart_refuse_nom_de_fichier_type_et_frontiere_invalides)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
