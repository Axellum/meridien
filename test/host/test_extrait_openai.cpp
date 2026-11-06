// Tests du noyau pur : extraction ciblée dans les réponses JSON OpenAI (extrait_openai.hpp).
#include "ecran/extrait_openai.hpp"
#include "mini.hpp"

using namespace ecran::assistant;

namespace {

constexpr bool delta_est(std::string_view json, Lecture lecture, std::string_view texte = std::string_view(),
                         std::size_t capacite = 256) {
    char b[256] = {};
    const Extrait e = contenu_du_delta(json, b, capacite > sizeof(b) ? sizeof(b) : capacite);
    return e.lecture == lecture && e.texte == texte;
}

constexpr bool transcription_est(std::string_view json, Lecture lecture, std::string_view texte = std::string_view(),
                                 std::size_t capacite = 256) {
    char b[256] = {};
    const Extrait e = texte_transcription(json, b, capacite > sizeof(b) ? sizeof(b) : capacite);
    return e.lecture == lecture && e.texte == texte;
}

constexpr bool erreur_est(std::string_view json, Lecture lecture, std::string_view texte = std::string_view(),
                          std::size_t capacite = 256) {
    char b[256] = {};
    const Extrait e = message_d_erreur(json, b, capacite > sizeof(b) ? sizeof(b) : capacite);
    return e.lecture == lecture && e.texte == texte;
}

// --- choices[0].delta.content ------------------------------------------------------------------------------

constexpr int morceau_de_reponse_ordinaire() {
    VERIFIER(delta_est(
        R"j({"id":"chatcmpl-1","object":"chat.completion.chunk","created":1,"model":"m","choices":[{"index":0,"delta":{"content":"Bonjour"},"finish_reason":null}]})j",
        Lecture::Texte, "Bonjour"));
    VERIFIER(delta_est(R"j({"choices":[{"delta":{"content":" le monde"}}]})j", Lecture::Texte, " le monde"));  // espaces gardés
    return 0;
}

constexpr int premier_et_dernier_morceaux() {
    // premier : rôle seul avec un contenu vide -> texte vide ; dernier : delta vide + finish_reason -> absent
    VERIFIER(delta_est(R"j({"choices":[{"index":0,"delta":{"role":"assistant","content":""},"finish_reason":null}]})j", Lecture::Texte, ""));
    VERIFIER(delta_est(R"j({"choices":[{"index":0,"delta":{"role":"assistant"}}]})j", Lecture::Absent));
    VERIFIER(delta_est(R"j({"choices":[{"index":0,"delta":{},"finish_reason":"stop"}]})j", Lecture::Absent));
    VERIFIER(delta_est(R"j({"choices":[{"delta":{"content":null}}]})j", Lecture::Absent));
    VERIFIER(delta_est(R"j({"choices":[],"usage":{"prompt_tokens":3,"completion_tokens":4}})j", Lecture::Absent));
    VERIFIER(delta_est(R"j({"choices":[{"delta":null}]})j", Lecture::Absent));
    VERIFIER(delta_est(R"j({"choices":null})j", Lecture::Absent));
    VERIFIER(delta_est(R"j({})j", Lecture::Absent));
    return 0;
}

constexpr int le_raisonnement_d_un_modele_local_n_est_jamais_lu() {
    VERIFIER(delta_est(R"j({"choices":[{"delta":{"reasoning_content":"je pense"}}]})j", Lecture::Absent));
    VERIFIER(delta_est(R"j({"choices":[{"delta":{"reasoning_content":"je pense","content":"voici"}}]})j", Lecture::Texte, "voici"));
    VERIFIER(delta_est(R"j({"choices":[{"delta":{"content":"voici","reasoning_content":"je pense"}}]})j", Lecture::Texte, "voici"));
    VERIFIER(delta_est(R"j({"choices":[{"delta":{"reasoning":"x","content_type":"text"}}]})j", Lecture::Absent));
    return 0;
}

constexpr int la_cle_content_ailleurs_n_est_pas_prise() {
    // dans une chaîne (guillemets échappés), dans un objet imbriqué, dans un autre champ de la racine
    VERIFIER(delta_est(R"j({"choices":[{"delta":{"note":"\"content\":\"faux\"","content":"vrai"}}]})j", Lecture::Texte, "vrai"));
    VERIFIER(delta_est(R"j({"choices":[{"delta":{"function_call":{"content":"non"},"content":"oui"}}]})j", Lecture::Texte, "oui"));
    VERIFIER(delta_est(R"j({"content":"racine","choices":[{"delta":{"content":"dedans"}}]})j", Lecture::Texte, "dedans"));
    VERIFIER(delta_est(R"j({"choices":[{"delta":{"tool_calls":[{"a":[1,2,{"content":"x"}]}]}}]})j", Lecture::Absent));
    VERIFIER(delta_est(R"j({"x":"}]}","choices":[{"delta":{"content":"ok"}}]})j", Lecture::Texte, "ok"));
    // des accolades dans une chaîne imbriquée d'une valeur sautée ne dérèglent pas le saut
    VERIFIER(delta_est(R"j({"meta":{"note":"}}]]","autre":["]"]},"choices":[{"delta":{"content":"ok"}}]})j", Lecture::Texte, "ok"));
    return 0;
}

constexpr int seul_le_premier_choix_est_lu() {
    VERIFIER(delta_est(R"j({"choices":[{"delta":{"content":"a"}},{"delta":{"content":"b"}}]})j", Lecture::Texte, "a"));
    VERIFIER(delta_est(R"j({"choices":[{"delta":{}},{"delta":{"content":"b"}}]})j", Lecture::Absent));
    return 0;
}

constexpr int blancs_toleres_entre_les_elements() {
    VERIFIER(delta_est(" { \"choices\" : [ { \"delta\" : { \"content\" : \"x\" } } ] } ", Lecture::Texte, "x"));
    VERIFIER(delta_est("{\n\t\"choices\":\r\n[{\"delta\":{\"content\":\"y\"}}]}", Lecture::Texte, "y"));
    return 0;
}

constexpr int echappements_json_decodes_en_utf8() {
    VERIFIER(delta_est(R"j({"choices":[{"delta":{"content":"a\"b\\c\/d\be\ff"}}]})j", Lecture::Texte, "a\"b\\c/d\be\ff"));
    VERIFIER(delta_est(R"j({"choices":[{"delta":{"content":"l1\nl2\r\n\tx"}}]})j", Lecture::Texte, "l1\nl2\r\n\tx"));
    VERIFIER(delta_est(R"j({"choices":[{"delta":{"content":"\u00e9\u00C9\u20ac\u0041"}}]})j", Lecture::Texte, "éÉ€A"));
    VERIFIER(delta_est(R"j({"choices":[{"delta":{"content":"\ud83d\ude00 \uD83D\uDE00"}}]})j", Lecture::Texte, "😀 😀"));  // paire de surrogates
    VERIFIER(delta_est(R"j({"choices":[{"delta":{"content":"l\u0027été"}}]})j", Lecture::Texte, "l'été"));
    VERIFIER(delta_est(R"j({"choices":[{"delta":{"content":"a\u0000b"}}]})j", Lecture::Texte, "ab"));  // NUL ignoré
    return 0;
}

constexpr int chiffres_hexadecimaux_en_majuscules_et_minuscules() {
    VERIFIER(delta_est(R"j({"choices":[{"delta":{"content":"\u00FF\u00fe\u00Fa\u00fA"}}]})j", Lecture::Texte, "ÿþúú"));
    VERIFIER(delta_est(R"j({"choices":[{"delta":{"content":"\u00G0"}}]})j", Lecture::Invalide));  // G n'est pas hexadécimal
    return 0;
}

constexpr int surrogates_isoles_deviennent_u_fffd() {
    VERIFIER(delta_est(R"j({"choices":[{"delta":{"content":"\ud83d"}}]})j", Lecture::Texte, "\xEF\xBF\xBD"));
    VERIFIER(delta_est(R"j({"choices":[{"delta":{"content":"\ude00"}}]})j", Lecture::Texte, "\xEF\xBF\xBD"));
    VERIFIER(delta_est(R"j({"choices":[{"delta":{"content":"\ud83dx"}}]})j", Lecture::Texte, "\xEF\xBF\xBD" "x"));
    VERIFIER(delta_est(R"j({"choices":[{"delta":{"content":"\ud83d\u0041"}}]})j", Lecture::Texte, "\xEF\xBF\xBD" "A"));
    VERIFIER(delta_est(R"j({"choices":[{"delta":{"content":"\ud83d\ud83d\ude00"}}]})j", Lecture::Texte, "\xEF\xBF\xBD" "😀"));
    return 0;
}

constexpr int utf8_brut_conserve_et_octets_invalides_remplaces() {
    VERIFIER(delta_est("{\"choices\":[{\"delta\":{\"content\":\"é€😀 ça\"}}]}", Lecture::Texte, "é€😀 ça"));
    VERIFIER(delta_est("{\"choices\":[{\"delta\":{\"content\":\"a\xFF" "b\"}}]}", Lecture::Texte, "a\xEF\xBF\xBD" "b"));
    VERIFIER(delta_est("{\"choices\":[{\"delta\":{\"content\":\"a\xE2\x82\"}}]}", Lecture::Texte, "a\xEF\xBF\xBD\xEF\xBF\xBD"));
    return 0;
}

constexpr int json_illisible_ou_de_mauvais_type_est_invalide() {
    VERIFIER(delta_est("", Lecture::Invalide));
    VERIFIER(delta_est("pas du json", Lecture::Invalide));
    VERIFIER(delta_est("[1,2]", Lecture::Invalide));
    VERIFIER(delta_est("{\"choices\":[{\"delta\":{\"content\":\"abc", Lecture::Invalide));  // tronqué dans la chaîne
    VERIFIER(delta_est("{\"choices\":[{\"delta\":{\"content\":", Lecture::Invalide));
    VERIFIER(delta_est("{\"choices\":[{\"delta\":{\"content\":5}}]}", Lecture::Invalide));
    VERIFIER(delta_est("{\"choices\":[{\"delta\":{\"content\":true}}]}", Lecture::Invalide));
    VERIFIER(delta_est("{\"choices\":[{\"delta\":{\"content\":{\"a\":1}}}]}", Lecture::Invalide));
    VERIFIER(delta_est("{\"choices\":[{\"delta\":{\"content\":\"\\q\"}}]}", Lecture::Invalide));      // échappement inconnu
    VERIFIER(delta_est("{\"choices\":[{\"delta\":{\"content\":\"\\u12\"}}]}", Lecture::Invalide));    // \u court
    VERIFIER(delta_est("{\"choices\":[{\"delta\":{\"content\":\"\\u12zz\"}}]}", Lecture::Invalide));  // \u non hexadécimal
    VERIFIER(delta_est("{\"choices\":{\"delta\":1}}", Lecture::Invalide));                           // choices pas un tableau
    VERIFIER(delta_est("{\"choices\":[3]}", Lecture::Invalide));
    VERIFIER(delta_est("{\"choices\":[{\"delta\":7}]}", Lecture::Invalide));
    VERIFIER(delta_est("{\"choices\"", Lecture::Invalide));
    VERIFIER(delta_est("{choices:1}", Lecture::Invalide));
    return 0;
}

constexpr int texte_trop_long_pour_le_tableau_n_est_jamais_tronque() {
    VERIFIER(delta_est(R"j({"choices":[{"delta":{"content":"abcd"}}]})j", Lecture::TropLong, "", 3));
    VERIFIER(delta_est(R"j({"choices":[{"delta":{"content":"abcd"}}]})j", Lecture::Texte, "abcd", 4));
    VERIFIER(delta_est(R"j({"choices":[{"delta":{"content":"é€"}}]})j", Lecture::TropLong, "", 4));  // é€ = 5 octets
    VERIFIER(delta_est(R"j({"choices":[{"delta":{"content":"é€"}}]})j", Lecture::Texte, "é€", 5));
    VERIFIER(delta_est(R"j({"choices":[{"delta":{"content":"\u20ac"}}]})j", Lecture::TropLong, "", 2));
    VERIFIER(delta_est(R"j({"choices":[{"delta":{"content":"x"}}]})j", Lecture::TropLong, "", 0));
    return 0;
}

// --- transcription -----------------------------------------------------------------------------------------

constexpr int transcription_ordinaire_et_rognee() {
    VERIFIER(transcription_est(R"j({"text":"Bonjour tout le monde."})j", Lecture::Texte, "Bonjour tout le monde."));
    VERIFIER(transcription_est(R"j({"text":" Bonjour.\n"})j", Lecture::Texte, "Bonjour."));  // whisper.cpp met des espaces
    VERIFIER(transcription_est(R"j({"text":"\t Quelle heure est-il ? \r\n"})j", Lecture::Texte, "Quelle heure est-il ?"));
    VERIFIER(transcription_est(R"j({"text":""})j", Lecture::Texte, ""));
    VERIFIER(transcription_est(R"j({"text":"   \n"})j", Lecture::Texte, ""));  // silence : texte vide -> RienReconnu
    VERIFIER(transcription_est(R"j({"text":"l\u0027été à \u00e9t\u00e9"})j", Lecture::Texte, "l'été à été"));
    return 0;
}

constexpr int transcription_cherche_text_au_premier_niveau_seulement() {
    VERIFIER(transcription_est(R"j({"usage":{"type":"tokens","text":"non"},"text":"oui"})j", Lecture::Texte, "oui"));
    VERIFIER(transcription_est(R"j({"segments":[{"text":"non"}],"text":"oui","language":"fr"})j", Lecture::Texte, "oui"));
    VERIFIER(transcription_est(R"j({"usage":{"text":"non"}})j", Lecture::Absent));
    VERIFIER(transcription_est(R"j({})j", Lecture::Absent));
    VERIFIER(transcription_est(R"j({"text":null})j", Lecture::Absent));
    VERIFIER(transcription_est(R"j({"text":5})j", Lecture::Invalide));
    VERIFIER(transcription_est("", Lecture::Invalide));
    VERIFIER(transcription_est("Bonjour.", Lecture::Invalide));  // format « text » brut : non demandé, non lu
    VERIFIER(transcription_est(R"j({"text":"abcd"})j", Lecture::TropLong, "", 3));
    return 0;
}

// --- error.message -----------------------------------------------------------------------------------------

constexpr int message_d_erreur_openai() {
    VERIFIER(erreur_est(
        R"j({"error":{"message":"Incorrect API key provided: sk-factice.","type":"invalid_request_error","param":null,"code":"invalid_api_key"}})j",
        Lecture::Texte, "Incorrect API key provided: sk-factice."));
    VERIFIER(erreur_est(R"j({"error":{"code":"model_not_found","message":"The model `x` does not exist"}})j", Lecture::Texte,
                        "The model `x` does not exist"));
    return 0;
}

constexpr int autres_formes_de_corps_d_erreur() {
    VERIFIER(erreur_est(R"j({"error":"model 'x' not found"})j", Lecture::Texte, "model 'x' not found"));  // Ollama
    VERIFIER(erreur_est(R"j([{"error":{"code":404,"message":"Not found","status":"NOT_FOUND"}}])j", Lecture::Texte, "Not found"));
    VERIFIER(erreur_est(R"j([ { "error" : { "message" : "Oups" } } ])j", Lecture::Texte, "Oups"));
    VERIFIER(erreur_est(R"j({"error":{"code":404}})j", Lecture::Absent));
    VERIFIER(erreur_est(R"j({"error":{"message":null}})j", Lecture::Absent));
    VERIFIER(erreur_est(R"j({"error":null})j", Lecture::Absent));
    VERIFIER(erreur_est(R"j({"detail":"autre"})j", Lecture::Absent));
    VERIFIER(erreur_est(R"j([])j", Lecture::Absent));
    VERIFIER(erreur_est(R"j({"other":{"error":{"message":"non"}}})j", Lecture::Absent));
    VERIFIER(erreur_est(R"j({"error":5})j", Lecture::Invalide));
    VERIFIER(erreur_est("<html><body>502 Bad Gateway</body></html>", Lecture::Invalide));
    VERIFIER(erreur_est("", Lecture::Invalide));
    VERIFIER(erreur_est(R"j({"error":{"message":"abc"}})j", Lecture::TropLong, "", 2));
    return 0;
}

constexpr int le_message_d_erreur_parle_t_il_du_modele() {
    VERIFIER(mentionne_modele("The model `gpt-9` does not exist"));
    VERIFIER(mentionne_modele("model 'x' not found, try pulling it first"));
    VERIFIER(mentionne_modele("Model not found"));
    VERIFIER(mentionne_modele("MODEL_NOT_FOUND"));
    VERIFIER(mentionne_modele("Modèle introuvable"));
    VERIFIER(mentionne_modele("le modèle n'est pas chargé"));
    VERIFIER(mentionne_modele("Unknown models list"));
    VERIFIER(!mentionne_modele("Incorrect API key provided"));
    VERIFIER(!mentionne_modele("Rate limit reached for requests"));
    VERIFIER(!mentionne_modele("mod"));
    VERIFIER(!mentionne_modele("mode"));
    VERIFIER(!mentionne_modele(""));
    return 0;
}

constexpr int raccord_avec_la_machine_a_etats_par_la_cause() {
    // Chaîne complète d'un 404 : le corps dit que le modèle n'existe pas -> phrase du modèle, pas celle du /v1.
    char b[128] = {};
    const Extrait e = message_d_erreur(R"j({"error":{"message":"The model `x` does not exist"}})j", b, sizeof(b));
    VERIFIER(e.lecture == Lecture::Texte && mentionne_modele(e.texte));
    char c[128] = {};
    const Extrait f = message_d_erreur(R"j({"detail":"Not Found"})j", c, sizeof(c));
    VERIFIER(f.lecture == Lecture::Absent && !mentionne_modele(f.texte));
    return 0;
}

}  // namespace

#define LISTE(X)                                                       \
    X(morceau_de_reponse_ordinaire)                                    \
    X(premier_et_dernier_morceaux)                                     \
    X(le_raisonnement_d_un_modele_local_n_est_jamais_lu)               \
    X(la_cle_content_ailleurs_n_est_pas_prise)                         \
    X(seul_le_premier_choix_est_lu)                                    \
    X(blancs_toleres_entre_les_elements)                               \
    X(echappements_json_decodes_en_utf8)                               \
    X(chiffres_hexadecimaux_en_majuscules_et_minuscules)               \
    X(surrogates_isoles_deviennent_u_fffd)                             \
    X(utf8_brut_conserve_et_octets_invalides_remplaces)                \
    X(json_illisible_ou_de_mauvais_type_est_invalide)                  \
    X(texte_trop_long_pour_le_tableau_n_est_jamais_tronque)            \
    X(transcription_ordinaire_et_rognee)                               \
    X(transcription_cherche_text_au_premier_niveau_seulement)          \
    X(message_d_erreur_openai)                                         \
    X(autres_formes_de_corps_d_erreur)                                 \
    X(le_message_d_erreur_parle_t_il_du_modele)                        \
    X(raccord_avec_la_machine_a_etats_par_la_cause)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
