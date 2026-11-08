'use strict';
/* Page de réglages (esquisse B, choisie par Axel le 06/11/2026). JS sans framework : le contrat est docs/api-reglages.md.
   Règles : une clé API est écrite mais jamais lue ; les erreurs s'affichent en phrases, jamais en code.
   Aucun texte d'écran n'est écrit ici : tout passe par t('identifiant'). Les textes vivent dans lang/<code>.json, un petit
   fichier par langue chargé à la demande, généré depuis i18n/page.yaml et i18n/<code>.yaml (voir i18n/README.md).
   Langue de la page = langue de l'écran de l'appareil ; repli : cette langue, puis l'anglais, puis le français. */
(() => {
const SVGNS = 'http://www.w3.org/2000/svg';
const $ = (s, r = document) => r.querySelector(s);
const attendre = (ms) => new Promise((r) => setTimeout(r, ms));
const NOMS = { stt: ['svc.stt', 'mic'], llm: ['svc.llm', 'chat'], tts: ['svc.tts', 'speaker'] };
const SECTIONS = ['ville', 'alertes', 'assistant', 'ecran', 'wifi', 'appareil'];
const S = { statut: null, systeme: null, zone: null, meteo: null, display: null, alertes: null, services: null, presets: null, wifi: null, maj: null, tests: {} };
let jeton = '';
try { jeton = sessionStorage.getItem('jeton') || ''; } catch (e) { /* stockage refusé : la page redemandera le code */ }

/* ---------- langues : un fichier JSON par langue, repli anglais puis français ---------- */
const LANGUE_PAR_DEFAUT = 'fr', LANGUE_DE_REPLI = 'en';
const CAT = { code: LANGUE_PAR_DEFAUT, mots: {} };
const lus = new Map();
async function lireCatalogue(code) {
  if (!/^[a-z]{2,3}$/.test(code)) return null;
  if (lus.has(code)) return lus.get(code);
  let c = null;
  try {
    const rep = await fetch('/lang/' + code + '.json');
    if (rep.ok) c = await rep.json();
  } catch (e) { /* fichier injoignable : on passe à la langue suivante */ }
  if (!c || typeof c !== 'object') return null;
  lus.set(code, c);
  return c;
}
/* Charge la langue demandée ; si son fichier manque, l'anglais, puis le français. Rend la langue réellement affichée. */
async function appliquerLangue(code) {
  for (const essai of new Set([code, LANGUE_DE_REPLI, LANGUE_PAR_DEFAUT])) {
    const c = await lireCatalogue(essai);
    if (!c) continue;
    CAT.code = essai;
    CAT.mots = c;
    document.documentElement.lang = essai;
    document.title = t('app.titre');
    return essai;
  }
  return null; /* aucun fichier : t() rend l'identifiant, visible et facile à repérer */
}
/* t('ville.titre') ; t('ville.fuseau', { fuseau, heure }) remplace les {noms} du texte. */
const t = (id, vars) => {
  const s = typeof CAT.mots[id] === 'string' ? CAT.mots[id] : id;
  return vars ? s.replace(/\{(\w+)\}/g, (m, k) => (k in vars ? String(vars[k]) : m)) : s;
};
/* Texte contenant des {noms} remplacés par des nœuds (un lien, par exemple) : rend une liste pour h(). */
const tNoeuds = (id, noeuds) => t(id).split(/(\{\w+\})/).map((m) => {
  const k = /^\{(\w+)\}$/.exec(m);
  return k && noeuds[k[1]] ? noeuds[k[1]] : m;
}).filter((m) => m !== '');
const pourcent = (v) => { try { return new Intl.NumberFormat(CAT.code, { style: 'percent' }).format(v / 100); } catch (e) { return v + '%'; } };

/* ---------- petits outils ---------- */
function h(tag, attrs, ...kids) {
  const e = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs || {})) {
    if (v == null || v === false) continue;
    if (k === 'class') e.className = v;
    else if (k === 'text') e.textContent = v;
    else if (k === 'value') e.value = v;
    else if (k.startsWith('on')) e.addEventListener(k.slice(2), v);
    else e.setAttribute(k, v === true ? '' : v);
  }
  for (const c of kids.flat()) if (c != null && c !== false) e.append(c.nodeType ? c : document.createTextNode(c));
  return e;
}
/* append() et replaceChildren() écrivent « null » à l'écran si on leur passe null : on écarte les vides avant. */
const nets = (...k) => k.flat().filter((c) => c != null && c !== false);
function ico(nom) {
  const s = document.createElementNS(SVGNS, 'svg');
  s.setAttribute('class', 'i');
  s.setAttribute('aria-hidden', 'true');
  const u = document.createElementNS(SVGNS, 'use');
  u.setAttribute('href', '#i-' + nom);
  s.append(u);
  return s;
}
const retour = (type, texte) => h('p', { class: 'res ' + type, role: type === 'err' ? 'alert' : 'status' },
  ico(type === 'ok' ? 'check' : type === 'err' ? 'cross' : 'info'), h('span', { text: texte }));
let tToast;
function toast(texte, erreur) {
  const el = $('#toast');
  el.className = erreur ? 'err' : '';
  el.replaceChildren(ico(erreur ? 'cross' : 'check'), texte);
  el.hidden = false;
  clearTimeout(tToast);
  tToast = setTimeout(() => { el.hidden = true; }, erreur ? 5000 : 2500);
}
const carte = (id, icone, titre, ...contenu) =>
  h('section', { class: 'carte', id, 'aria-labelledby': 't-' + id }, h('h2', { id: 't-' + id }, ico(icone), titre), ...contenu);
const remplacer = (id, nouvelle) => { const a = document.getElementById(id); if (a) a.replaceWith(nouvelle); return nouvelle; };

/* ---------- accès à l'API ---------- */
class Erreur extends Error {
  constructor(message, statut, id, champ) { super(message); this.statut = statut; this.id = id; this.champ = champ; }
}
/* Les phrases d'erreur de l'appareil arrivent déjà dans la langue de l'écran ; les nôtres viennent de t(). */
const msg = (e) => (e instanceof Erreur ? e.message : t('erreur.interne'));
async function api(methode, chemin, corps, signal) {
  const init = { method: methode, headers: {}, signal };
  if (jeton) init.headers.Authorization = 'Bearer ' + jeton;
  if (corps !== undefined) {
    init.headers['Content-Type'] = 'application/json';
    init.body = typeof corps === 'string' ? corps : JSON.stringify(corps);
  }
  let rep;
  try { rep = await fetch(chemin, init); } catch (e) {
    if (e.name === 'AbortError') throw e;
    throw new Erreur(t('erreur.injoignable'), 0, 'injoignable');
  }
  if (rep.status === 204) return null;
  const json = (rep.headers.get('content-type') || '').includes('json') ? await rep.json().catch(() => null) : null;
  if (rep.ok) return json;
  if (rep.status === 401) { oublier(); montrerAppairage(); }
  const er = json && json.erreur;
  throw new Erreur((er && er.message) || t('erreur.interne'), rep.status, er && er.id, er && er.champ);
}
function oublier() {
  jeton = '';
  try { sessionStorage.removeItem('jeton'); } catch (e) { /* sans importance */ }
}

/* ---------- appairage ---------- */
function montrerAppairage(message) {
  const retours = h('div');
  const champ = h('input', { id: 'code', inputmode: 'numeric', autocomplete: 'one-time-code', maxlength: '6', pattern: '[0-9]*', 'aria-label': t('appairage.code') });
  let enCours = false;
  const envoyer = async () => {
    const code = champ.value.replace(/\D/g, '');
    if (code.length !== 6 || enCours) return;
    enCours = true;
    retours.replaceChildren();
    try {
      const r = await api('POST', '/api/pair', { code });
      jeton = r.jeton;
      try { sessionStorage.setItem('jeton', jeton); } catch (e) { /* tant pis */ }
      demarrer();
    } catch (e) {
      retours.replaceChildren(retour('err', msg(e)));
      champ.value = '';
      champ.focus();
    } finally { enCours = false; }
  };
  champ.addEventListener('input', () => {
    champ.value = champ.value.replace(/\D/g, '').slice(0, 6);
    if (champ.value.length === 6) envoyer();
  });
  $('#racine').replaceChildren(h('main', { class: 'appairage' },
    h('span', { class: 'logo' }, ico('tablet')), h('h1', { text: t('app.titre') }),
    h('p', { text: message || t('appairage.invite') }),
    champ, retours, h('button', { type: 'button', class: 'btn', onclick: envoyer, text: t('appairage.continuer') })));
  champ.focus();
}

/* ---------- démarrage ---------- */
/* Langue de départ, avant de connaître celle de l'écran : celle du navigateur si un fichier existe, sinon le français. */
const langueDuNavigateur = () => ((navigator.language || '').slice(0, 2).toLowerCase()) || LANGUE_PAR_DEFAUT;
async function demarrer(ancre) {
  if (!CAT.mots['app.titre']) await appliquerLangue(langueDuNavigateur());
  let st;
  try { st = await api('GET', '/api/status'); } catch (e) {
    $('#racine').replaceChildren(h('main', { class: 'appairage' }, h('h1', { text: t('app.titre') }), retour('err', msg(e)),
      h('button', { type: 'button', class: 'btn', onclick: () => demarrer(), text: t('action.reessayer') })));
    return;
  }
  /* /api/status est public : il donne la langue de l'écran même avant l'appairage. */
  if (st.langue && st.langue !== CAT.code) await appliquerLangue(st.langue);
  if (st.appairage !== 'ok') { oublier(); montrerAppairage(); return; }
  S.statut = st;
  const [sys, zone, disp, svc, pre, wifi, maj, alr] = await Promise.allSettled(
    ['/api/system', '/api/location', '/api/display', '/api/services', '/api/presets', '/api/wifi', '/api/update', '/api/alerts'].map((c) => api('GET', c)));
  if (!jeton) return; /* une réponse 401 a déjà ramené à l'appairage */
  const val = (r) => (r.status === 'fulfilled' ? r.value : null);
  Object.assign(S, { systeme: val(sys), zone: val(zone), display: val(disp), alertes: val(alr), services: val(svc), presets: val(pre), wifi: val(wifi), maj: val(maj) });
  S.meteo = null;
  construirePage();
  if (ancre && document.getElementById(ancre)) document.getElementById(ancre).scrollIntoView();
  if (S.zone && S.zone.definie !== false) chargerMeteo(1);
}

function construirePage() {
  const nav = h('nav', { class: 'ancres', 'aria-label': t('nav.sections') },
    SECTIONS.map((id) => h('a', { href: '#' + id, text: t('nav.' + id), 'aria-current': id === 'ville' ? 'true' : 'false' })));
  $('#racine').replaceChildren(h('div', { class: 'page' },
    h('header', { class: 'haut' }, h('div', { class: 'top' }, h('span', { class: 'logo' }, ico('tablet')),
      h('div', {}, h('b', { text: t('app.titre') }), h('small', { id: 'sous-titre' }))), nav),
    h('main', {}, carteVille(), carteAlertes(), carteAssistant(), carteEcran(), carteWifi(), carteAppareil())));
  majSousTitre();
  const liens = [...nav.querySelectorAll('a')];
  const actif = (id) => liens.forEach((a) => a.setAttribute('aria-current', String(a.getAttribute('href') === '#' + id)));
  liens.forEach((a) => a.addEventListener('click', () => actif(a.getAttribute('href').slice(1))));
  if ('IntersectionObserver' in window) {
    const obs = new IntersectionObserver((es) => es.forEach((e) => { if (e.isIntersecting) actif(e.target.id); }), { rootMargin: '-25% 0px -65% 0px' });
    SECTIONS.forEach((id) => obs.observe(document.getElementById(id)));
  }
}
function majSousTitre() {
  const w = S.wifi, el = $('#sous-titre');
  if (!el) return;
  const nom = (S.statut && S.statut.nom) || t('app.nom_defaut');
  el.textContent = w && w.etat === 'connecte' && w.ssid ? t('entete.connecte', { nom, ssid: w.ssid }) : nom;
}

/* ---------- ville et météo ---------- */
let majVille = () => {};
let horloge;
const heureDe = (fuseau) => {
  try {
    const p = new Intl.DateTimeFormat(CAT.code, { timeZone: fuseau, hour: 'numeric', minute: '2-digit', hourCycle: 'h23' }).formatToParts(new Date());
    const g = (x) => (p.find((y) => y.type === x) || {}).value;
    return t('ville.heure', { h: g('hour'), m: g('minute') });
  } catch (e) { return ''; }
};
async function chargerMeteo(essais) {
  try {
    const m = await api('GET', '/api/weather');
    if (m && m.disponible) { S.meteo = m; majVille(); return; }
  } catch (e) { /* météo indisponible : la carte le dit */ }
  if (essais < 4) { await attendre(1500); chargerMeteo(essais + 1); } else { S.meteo = { disponible: false }; majVille(); }
}
function carteVille() {
  const hero = h('div');
  const aide = h('p', { class: 'aide' });
  const etiquette = h('label', { class: 'lab', for: 'ville-q' });
  const liste = h('ul', { class: 'resultats', hidden: true });
  const retours = h('div', { 'aria-live': 'polite' });
  const champ = h('input', { id: 'ville-q', type: 'search', placeholder: t('ville.recherche_ph'), autocomplete: 'off', autocapitalize: 'off', spellcheck: 'false', enterkeyhint: 'search' });
  /* Attribution exigée par les licences des données (Open-Meteo : CC BY 4.0 ; MET Norway : CC BY 4.0 et NLOD 2.0, données adaptées ;
     NWS : données ouvertes, sans parrainage) : toujours visible, dans la langue de la page. Elle nomme le service qui a RÉELLEMENT donné
     la météo montrée (`fournisseur` de /api/weather), pas celui qui est choisi : quand un service lu directement n'a pas répondu, la météo
     vient d'Open-Meteo et c'est lui qu'il faut citer (ADR-0014). Sans météo reçue, c'est Open-Meteo, comme avant. */
  const attribution = h('p', { class: 'aide attribution', id: 'attribution' });
  /* Source de la météo : la liste vient de l'appareil et ne contient que les sources qui couvrent le pays de la ville (ADR-0010) ;
     « Automatique » est toujours là. Le modèle choisi est nommé à côté de l'attribution, là où la source est montrée. */
  const modele = h('span', { id: 'attribution-modele' });
  const PHRASES = { 'open-meteo': 'meteo.attribution', 'met-norway': 'meteo.attribution_metno', nws: 'meteo.attribution_nws' };
  const PAR_DEFAUT = { id: 'open-meteo', nom: 'Open-Meteo', lien: 'https://open-meteo.com/' };
  /* Les noms de licence de la phrase sont des liens vers leur texte : CC BY 4.0 demande le texte de la licence ou un lien vers elle, NLOD 2.0
     un lien vers la licence quand c'est possible (relu mot à mot le 08/11/2026). Ces deux noms s'écrivent pareil dans toutes les langues du
     catalogue ; l'écran de l'appareil ne peut pas porter de lien, la page oui. Adresses fixes, https seulement. */
  const LICENCES = { 'CC BY 4.0': 'https://creativecommons.org/licenses/by/4.0/', 'NLOD 2.0': 'https://data.norge.no/nlod/en/2.0' };
  const liensLicences = (morceaux) => morceaux.flatMap((m) => (typeof m !== 'string' ? [m]
    : m.split(/(CC BY 4\.0|NLOD 2\.0)/).filter((x) => x !== '').map((x) => (LICENCES[x] ? h('a', { href: LICENCES[x], target: '_blank', rel: 'noopener', text: x }) : x))));
  let cleAttribution = '';
  const majAttribution = () => {
    const f = S.meteo && S.meteo.fournisseur;
    const sur = f && PHRASES[f.id] && typeof f.nom === 'string' && /^https:\/\//.test(f.lien) ? f : PAR_DEFAUT; /* le lien vient de l'appareil : https seulement */
    const cle = sur.id;
    if (cle === cleAttribution) return;
    cleAttribution = cle;
    attribution.replaceChildren(...nets(liensLicences(tNoeuds(PHRASES[sur.id], { lien: h('a', { href: sur.lien, target: '_blank', rel: 'noopener', text: sur.nom }) })), modele,
      sur.id === 'nws' ? ' ' + t('meteo.sans_parrainage') : null));
  };
  const source = h('select', { id: 'source-meteo', 'aria-describedby': 'source-meteo-aide', onchange: () => choisirSource(source.value) });
  const sourceRetablie = retour('info', t('source.retablie'));
  sourceRetablie.id = 'source-retablie';
  const sourceRepliTexte = h('span');
  const sourceRepli = h('p', { class: 'res info', role: 'status', id: 'source-repli', hidden: true }, ico('info'), sourceRepliTexte);
  const champSource = h('div', { class: 'f', id: 'source-champ' },
    h('label', { class: 'lab', for: 'source-meteo', text: t('source.titre') }), h('div', { class: 'champ' }, source, ico('chevd')),
    h('p', { class: 'aide', id: 'source-meteo-aide', text: t('source.aide') + ' ' + t('source.filtre') }), sourceRetablie, sourceRepli);
  const majSource = () => {
    const z = S.zone, liste = z && Array.isArray(z.sources_meteo) ? z.sources_meteo : [];
    champSource.hidden = !liste.length;
    source.replaceChildren(...liste.map((s) => h('option', { value: s.id, text: s.nom, selected: s.id === z.source_meteo })));
    sourceRetablie.hidden = !(z && z.source_meteo_retablie);
    /* « Modèle : … » seulement pour un modèle d'Open-Meteo : un service lu directement (MET Norway, NWS) n'est pas un modèle. */
    const choisie = z && liste.find((s) => s.id === z.source_meteo);
    modele.textContent = z && z.source_meteo && z.source_meteo !== 'auto' && choisie && !choisie.direct ? ' · ' + t('source.modele', { modele: z.source_meteo_nom }) : '';
  };
  /* Le service choisi n'a pas répondu : la météo vient d'Open-Meteo en attendant, et la page le dit (l'écran dit la même chose).
     À part de majSource : celle-ci refait la liste déroulante, et la météo se met à jour toute seule pendant qu'on choisit. */
  const majRepli = () => {
    const repli = !!(S.meteo && S.meteo.repli && S.zone);
    sourceRepli.hidden = !repli;
    sourceRepliTexte.textContent = repli ? t('source.repli', { service: S.zone.source_meteo_nom }) : '';
  };
  majVille = () => {
    const z = S.zone, def = z && z.definie !== false;
    majAttribution(); majRepli(); /* la météo reçue (ou effacée) change le service nommé et le message de repli */
    etiquette.textContent = def ? t('ville.changer') : t('ville.choisir');
    if (!def) {
      hero.replaceChildren(h('div', { class: 'hero' }, h('div', {}, h('small', { text: t('ville.aucune') }), h('b', { class: 'ville', text: t('ville.choisissez') }))));
      aide.textContent = t('ville.aide_aucune');
      return;
    }
    const m = S.meteo && S.meteo.disponible ? S.meteo.actuelle : null;
    hero.replaceChildren(h('div', { class: 'hero' },
      h('div', {}, h('small', { text: t('ville.meteo_a') }), h('b', { class: 'ville', text: z.nom }), h('small', { text: [z.region, z.pays].filter(Boolean).join(' · ') })),
      h('div', { class: 'temp' }, m ? [h('b', { text: `${Math.round(m.temperature)} °C` }), h('small', { text: m.libelle })]
        : h('small', { text: S.meteo ? t('ville.meteo_indispo') : t('ville.meteo_encours') }))));
    aide.textContent = t('ville.fuseau', { fuseau: z.fuseau, heure: heureDe(z.fuseau) });
  };
  let minuteur, ctrl, seq = 0;
  const chercher = async (q) => {
    if (ctrl) ctrl.abort();
    ctrl = new AbortController();
    const mon = ++seq;
    retours.replaceChildren(retour('info', t('ville.recherche')));
    try {
      const r = await api('GET', '/api/geocode?q=' + encodeURIComponent(q), undefined, ctrl.signal);
      if (mon !== seq) return;
      liste.replaceChildren(...r.resultats.map((l) => h('li', {}, h('button', { type: 'button', class: 'lieu', onclick: () => choisir(l) },
        ico('pin'), h('div', {}, h('b', { text: l.nom }), h('small', { text: [l.region, l.pays].filter(Boolean).join(' · ') }))))));
      liste.hidden = !r.resultats.length;
      retours.replaceChildren(r.resultats.length ? '' : retour('info', t('ville.aucun_lieu', { q })));
    } catch (e) {
      if (e.name === 'AbortError' || mon !== seq) return;
      liste.hidden = true;
      retours.replaceChildren(retour('err', msg(e)));
    }
  };
  champ.addEventListener('input', () => {
    clearTimeout(minuteur);
    if (ctrl) ctrl.abort();
    seq++;
    const q = champ.value.trim();
    liste.replaceChildren(); liste.hidden = true;
    retours.replaceChildren(q && q.length < 3 ? h('p', { class: 'aide', text: t('ville.min3') }) : '');
    if (q.length >= 3) minuteur = setTimeout(() => chercher(q), 350);
  });
  /* Changer la source se passe comme changer de ville : l'appareil efface la météo gardée et en redemande une. */
  const choisirSource = async (id) => {
    source.disabled = true;
    try {
      S.zone = await api('PUT', '/api/location', { source_meteo: id });
      retours.replaceChildren();
      S.meteo = null; majVille();
      toast(t('source.enregistree', { source: S.zone.source_meteo_nom }));
      chargerMeteo(1);
    } catch (e) {
      retours.replaceChildren(retour('err', msg(e)));
    }
    source.disabled = false;
    majSource(); /* la liste montre toujours ce que l'appareil a vraiment retenu */
  };
  const choisir = async (lieu) => {
    liste.querySelectorAll('button').forEach((b) => { b.disabled = true; });
    try {
      S.zone = await api('PUT', '/api/location', lieu);
      champ.value = ''; liste.replaceChildren(); liste.hidden = true; retours.replaceChildren();
      S.meteo = null; majVille(); majSource();
      toast(t('ville.enregistree', { ville: S.zone.nom }));
      chargerMeteo(1);
    } catch (e) {
      liste.querySelectorAll('button').forEach((b) => { b.disabled = false; });
      retours.replaceChildren(retour('err', msg(e)));
    }
  };
  clearInterval(horloge);
  horloge = setInterval(majVille, 30000);
  majVille();
  majSource();
  return carte('ville', 'pin', t('ville.titre'), hero, aide, h('div', { class: 'f' }, etiquette, h('div', { class: 'champ' }, ico('search'), champ)), retours, liste, champSource, attribution);
}

/* ---------- assistant : trois services ---------- */
function carteAssistant() {
  const blocs = ['stt', 'llm', 'tts'].map(blocService);
  blocs.forEach((b) => b.entete.addEventListener('click', () => blocs.forEach((x) => x.ouvrir(x === b && !x.estOuvert()))));
  return carte('assistant', 'mic', t('assistant.titre'),
    h('p', { class: 'aide', text: t('assistant.aide') }),
    ...blocs.flatMap((b) => [b.entete, b.detail]));
}
function blocService(id) {
  const [idTitre, icone] = NOMS[id];
  const titre = t(idTitre);
  const presets = (S.presets && S.presets[id]) || [];
  let svc = (S.services && S.services[id]) || { configure: false, cle_definie: false };
  const v = { preset: svc.preset || '', url: svc.url || '', modele: svc.modele || '', voix: svc.voix || '', cle: '', remplacerCle: false, effacerCle: false };
  let modifie = false;
  const sous = h('small');
  const pastille = h('span', { class: 'pill' });
  const entete = h('button', { type: 'button', class: 'rang', 'aria-expanded': 'false', 'aria-controls': 'det-' + id },
    ico(icone), h('div', {}, h('b', { text: titre }), sous), pastille, ico('chev'));
  const detail = h('div', { class: 'detail', id: 'det-' + id, hidden: true });
  const nomPreset = (p) => { const x = presets.find((q) => q.id === p); return x ? x.nom : t('svc.personnalise'); };
  const majEntete = () => {
    const test = S.tests[id];
    sous.textContent = svc.configure ? t(svc.cle_definie ? 'svc.avec_cle' : 'svc.sans_cle', { service: nomPreset(svc.preset) }) : t('svc.pas_regle');
    pastille.className = 'pill' + (test ? (test.ok ? ' ok' : ' err') : '');
    pastille.textContent = test ? t(test.ok ? 'pill.pret' : 'pill.verifier') : t(svc.configure ? 'pill.regle' : 'pill.a_regler');
  };
  /* formulaire */
  const champs = {};
  const rangee = (cle, label, attrs, valeur) => {
    const input = h('input', { id: `${id}-${cle}`, autocomplete: 'off', autocapitalize: 'off', spellcheck: 'false', ...attrs, value: valeur });
    input.addEventListener('input', () => { v[cle] = input.value; input.removeAttribute('aria-invalid'); touche(); });
    champs[cle] = input;
    return h('div', { class: 'f' }, h('label', { class: 'lab', for: input.id, text: label }), h('div', { class: 'champ' }, input));
  };
  const chips = h('div', { class: 'chips', role: 'radiogroup', 'aria-label': t('svc.ou_tourne') });
  const aidePreset = h('p', { class: 'aide' });
  const dessinerChips = () => {
    chips.replaceChildren(...presets.map((p) => h('button', { type: 'button', class: 'chip', role: 'radio', 'aria-checked': String(v.preset === p.id), onclick: () => choisirPreset(p) },
      v.preset === p.id ? ico('check') : null, p.nom)));
    const p = presets.find((x) => x.id === v.preset);
    aidePreset.replaceChildren(p ? [p.aide || '', p.local ? ' ' + t('svc.local_aide') : ''].join('').trim() : '');
    aidePreset.hidden = !aidePreset.textContent;
  };
  const choisirPreset = (p) => {
    v.preset = p.id;
    if (p.url) { v.url = p.url; champs.url.value = p.url; }
    champs.url.removeAttribute('aria-invalid');
    dessinerChips(); dessinerCle(); touche();
  };
  const champUrl = rangee('url', t('svc.adresse'), { type: 'url', inputmode: 'url', placeholder: 'https://…' }, v.url);
  const datalist = h('datalist', { id: `mod-${id}` });
  const champModele = rangee('modele', t('svc.modele'), { list: `mod-${id}`, placeholder: t('svc.modele_ph') }, v.modele);
  const champsExtra = id === 'tts' ? rangee('voix', t('svc.voix'), { placeholder: t('svc.facultatif') }, v.voix) : null;
  /* clé : écriture seule */
  const cleEl = h('div', { class: 'f' });
  const dessinerCle = () => {
    const p = presets.find((x) => x.id === v.preset);
    const lab = h('span', { class: 'lab', text: t('svc.cle') });
    let corps;
    if (svc.cle_definie && !v.remplacerCle && !v.effacerCle) {
      corps = h('div', {}, h('div', { class: 'champ cle' }, ico('key'), h('span', { class: 'val', text: t('svc.cle_enregistree') })),
        h('div', { class: 'liens' },
          h('button', { type: 'button', class: 'lien', onclick: () => { v.remplacerCle = true; dessinerCle(); champs.cle.focus(); }, text: t('action.remplacer') }),
          h('button', { type: 'button', class: 'lien', onclick: () => { v.effacerCle = true; dessinerCle(); touche(); }, text: t('action.effacer') })));
    } else if (v.effacerCle) {
      corps = h('div', {}, retour('info', t('svc.cle_effacee')),
        h('button', { type: 'button', class: 'lien', onclick: () => { v.effacerCle = false; dessinerCle(); touche(); }, text: t('action.annuler') }));
    } else {
      const input = h('input', { id: `${id}-cle`, type: 'password', autocomplete: 'off', autocapitalize: 'off', spellcheck: 'false', 'data-lpignore': 'true',
        placeholder: p && p.cle_requise === false ? t('svc.cle_facultative') : t('svc.cle_coller'), value: v.cle });
      input.addEventListener('input', () => { v.cle = input.value; input.removeAttribute('aria-invalid'); touche(); });
      champs.cle = input;
      corps = h('div', {}, h('div', { class: 'champ' }, ico('key'), input),
        svc.cle_definie ? h('button', { type: 'button', class: 'lien', onclick: () => { v.remplacerCle = false; v.cle = ''; dessinerCle(); touche(); }, text: t('svc.cle_garder') }) : null);
    }
    cleEl.replaceChildren(lab, corps, h('p', { class: 'aide', text: t('svc.cle_aide') }));
  };
  /* actions */
  const resTest = h('div', { 'aria-live': 'polite' });
  const resSave = h('div', { 'aria-live': 'polite' });
  const btnTest = h('button', { type: 'button', class: 'btn sec', onclick: () => tester() }, ico('play'), t('action.tester'));
  const btnSave = h('button', { type: 'button', class: 'btn', onclick: () => enregistrer(), text: t('action.enregistrer') });
  const touche = () => { modifie = true; majBoutons(); };
  const majBoutons = () => { btnSave.disabled = !v.url.trim() || !(modifie || !svc.configure); };
  /* La langue parlée n'est plus un champ du service : elle suit la « langue de l'assistant » (réglage de l'écran). */
  const corpsService = () => {
    const c = { url: v.url, modele: v.modele };
    if (id === 'tts') c.voix = v.voix;
    return c;
  };
  async function tester() {
    btnTest.disabled = true;
    resTest.replaceChildren(retour('info', t('svc.test_encours')));
    const c = corpsService();
    if (v.cle) c.cle = v.cle;
    try {
      await api('POST', `/api/services/${id}/test`, c);
      let r = null;
      for (let i = 0; i < 40; i++) {
        await attendre(i ? 700 : 300);
        r = await api('GET', `/api/services/${id}/test`);
        if (r.etat === 'termine') break;
      }
      if (!r || r.etat !== 'termine') throw new Erreur(t('erreur.test_long'), 0, 'trop_long');
      S.tests[id] = r;
      const n = (r.modeles || []).length;
      if (n) datalist.replaceChildren(...r.modeles.map((m) => h('option', { value: m })));
      const idModeles = n > 1 ? (v.modele ? 'svc.modeles_n' : 'svc.modeles_n_choisir') : (v.modele ? 'svc.modeles_1' : 'svc.modeles_1_choisir');
      resTest.replaceChildren(...nets(retour(r.ok ? 'ok' : 'err', r.message),
        r.ok && n ? h('p', { class: 'aide', text: t(idModeles, { n }) }) : null));
    } catch (e) { resTest.replaceChildren(retour('err', msg(e))); } finally { btnTest.disabled = false; majEntete(); }
  }
  async function enregistrer() {
    const c = { preset: v.preset || 'personnalise', ...corpsService() };
    if (v.cle) c.cle = v.cle; else if (v.effacerCle) c.effacer_cle = true;
    btnSave.disabled = true;
    try {
      svc = await api('PUT', `/api/services/${id}`, c);
      S.services = { ...(S.services || {}), [id]: svc };
      Object.assign(v, { cle: '', remplacerCle: false, effacerCle: false });
      modifie = false;
      resSave.replaceChildren();
      dessinerCle(); majEntete(); majBoutons();
      toast(t('toast.enregistre'));
    } catch (e) {
      resSave.replaceChildren(retour('err', msg(e)));
      if (e.champ && champs[e.champ]) champs[e.champ].setAttribute('aria-invalid', 'true');
      majBoutons();
    }
  }
  detail.append(...nets(h('div', { class: 'f' }, h('span', { class: 'lab', text: t('svc.ou_tourne') }), chips, aidePreset),
    champUrl, champModele, datalist, champsExtra, cleEl, h('div', { class: 'btns' }, btnTest, btnSave), resTest, resSave));
  dessinerChips(); dessinerCle(); majEntete(); majBoutons();
  const ouvrir = (oui) => {
    entete.setAttribute('aria-expanded', String(oui));
    detail.hidden = !oui;
    entete.lastChild.replaceWith(ico(oui ? 'chevd' : 'chev'));
  };
  return { entete, detail, ouvrir, estOuvert: () => !detail.hidden };
}

/* ---------- alertes officielles (Vigie) ---------- */
const minutes = (v) => { try { return new Intl.NumberFormat(CAT.code, { style: 'unit', unit: 'minute', unitDisplay: 'short' }).format(v); } catch (e) { return v + ' min'; } };
const NIVEAUX_ALERTE = { yellow: () => t('alertes.jaune'), orange: () => t('alertes.orange'), red: () => t('alertes.rouge') };
function carteAlertes() {
  const a = S.alertes;
  if (!a) return carte('alertes', 'bell', t('alertes.titre'), retour('err', t('erreur.interne')));
  const enregistrer = async (champ) => {
    try { S.alertes = await api('PUT', '/api/alerts', champ); toast(t('toast.enregistre')); } catch (e) { toast(msg(e), true); remplacer('alertes', carteAlertes()); }
  };
  /* Un groupe de boutons (oui / non, jaune / orange / rouge) : le choix part tout de suite, comme le thème de l'écran. */
  const groupe = (id, libelle, aide, options, courant, champ) => {
    const boutons = options.map(([valeur, texte]) => {
      const b = h('button', { type: 'button', role: 'radio', 'aria-checked': String(valeur === courant), text: texte });
      b.addEventListener('click', () => { boutons.forEach((x) => x.setAttribute('aria-checked', String(x === b))); enregistrer({ [champ]: valeur }); });
      return b;
    });
    return h('div', { class: 'f' }, h('span', { class: 'lab', id: id + '-l', text: libelle }),
      h('div', { class: 'seg', role: 'radiogroup', 'aria-labelledby': id + '-l', 'aria-describedby': id + '-a' }, boutons),
      h('p', { class: 'aide', id: id + '-a', text: aide }));
  };
  const niveaux = (a.niveaux || Object.keys(NIVEAUX_ALERTE)).filter((n) => n in NIVEAUX_ALERTE);
  const valeur = h('span', { class: 'valeur', text: minutes(a.periode_min) });
  const periode = h('input', { id: 'alertes-periode', type: 'range', min: '10', max: '180', step: '1', value: String(a.periode_min), 'aria-valuetext': minutes(a.periode_min), 'aria-describedby': 'alertes-periode-aide' });
  periode.addEventListener('input', () => { valeur.textContent = minutes(periode.value); periode.setAttribute('aria-valuetext', minutes(periode.value)); });
  periode.addEventListener('change', () => enregistrer({ periode_min: Number(periode.value) }));
  /* L'adresse est contrôlée par l'appareil (https:// obligatoire) : sa phrase d'erreur s'affiche sous le champ, et la valeur
     enregistrée (normalisée) remplace celle qui a été tapée. Vide = le service public, dont l'adresse sert d'exemple grisé. */
  const adresse = h('input', { id: 'alertes-adresse', type: 'text', inputmode: 'url', autocomplete: 'off', autocapitalize: 'off', spellcheck: 'false', maxlength: '80', placeholder: a.adresse_publique, value: a.adresse, 'aria-describedby': 'alertes-adresse-aide' });
  const retoursAdresse = h('div', { 'aria-live': 'polite' });
  adresse.addEventListener('change', async () => {
    adresse.removeAttribute('aria-invalid');
    retoursAdresse.replaceChildren();
    try {
      S.alertes = await api('PUT', '/api/alerts', { adresse: adresse.value });
      adresse.value = S.alertes.adresse;
      toast(t('toast.enregistre'));
    } catch (e) {
      adresse.setAttribute('aria-invalid', 'true');
      retoursAdresse.replaceChildren(retour('err', msg(e)));
    }
  });
  return carte('alertes', 'bell', t('alertes.titre'),
    groupe('alertes-actives', t('alertes.actives'), t('alertes.actives_aide'), [[true, t('alertes.oui')], [false, t('alertes.non')]], a.actives, 'actives'),
    groupe('alertes-niveau', t('alertes.niveau'), t('alertes.niveau_aide'), niveaux.map((n) => [n, NIVEAUX_ALERTE[n]()]), a.niveau_min, 'niveau_min'),
    h('div', { class: 'f' }, h('label', { class: 'lab', for: 'alertes-periode' }, t('alertes.periode'), valeur), periode,
      h('p', { class: 'aide', id: 'alertes-periode-aide', text: t('alertes.periode_aide') })),
    h('div', { class: 'f' }, h('label', { class: 'lab', for: 'alertes-adresse', text: t('alertes.adresse') }), h('div', { class: 'champ' }, adresse),
      h('p', { class: 'aide', id: 'alertes-adresse-aide', text: t('alertes.adresse_aide') }), retoursAdresse));
}

/* ---------- écran ---------- */
function carteEcran() {
  const d = S.display;
  if (!d) return carte('ecran', 'sun', t('ecran.titre'), retour('err', t('erreur.ecran_illisible')));
  const enregistrer = async (champ) => {
    try { S.display = await api('PUT', '/api/display', champ); toast(t('toast.enregistre')); } catch (e) { toast(msg(e), true); remplacer('ecran', carteEcran()); }
  };
  /* Changer la langue de l'écran change aussi celle de cette page et des phrases que l'appareil renvoie : on recharge tout. */
  const changerLangueEcran = async (code) => {
    try { S.display = await api('PUT', '/api/display', { langue_ecran: code }); } catch (e) { toast(msg(e), true); remplacer('ecran', carteEcran()); return; }
    await appliquerLangue(code);
    await demarrer('ecran');
    toast(t('toast.enregistre'));
  };
  const options = (liste, courant) => (liste || []).map((l) => h('option', { value: l.code, text: l.nom, selected: l.code === courant }));
  const langueEcran = h('select', { id: 'langue-ecran', onchange: () => changerLangueEcran(langueEcran.value) }, options(d.langues, d.langue_ecran));
  const langueAssistant = h('select', { id: 'langue-assistant', 'aria-describedby': 'langue-assistant-aide', onchange: () => enregistrer({ langue_assistant: langueAssistant.value }) },
    options(d.langues_assistant || d.langues, d.langue_assistant));
  const THEMES = ['auto', 'clair', 'sombre'];
  const seg = h('div', { class: 'seg', role: 'radiogroup', 'aria-label': t('ecran.theme') }, (d.themes || THEMES).map((th) =>
    h('button', { type: 'button', role: 'radio', 'aria-checked': String(th === d.theme), text: t('theme.' + th), onclick: () => {
      seg.querySelectorAll('button').forEach((b) => b.setAttribute('aria-checked', String(b === seg.querySelector(`[data-t="${th}"]`))));
      enregistrer({ theme: th });
    }, 'data-t': th })));
  const valeur = h('span', { class: 'valeur', text: pourcent(d.luminosite) });
  const lum = h('input', { id: 'luminosite', type: 'range', min: '5', max: '100', step: '1', value: String(d.luminosite), 'aria-valuetext': pourcent(d.luminosite) });
  lum.addEventListener('input', () => { valeur.textContent = pourcent(lum.value); lum.setAttribute('aria-valuetext', pourcent(lum.value)); });
  lum.addEventListener('change', () => enregistrer({ luminosite: Number(lum.value) }));
  return carte('ecran', 'sun', t('ecran.titre'),
    h('div', { class: 'f' }, h('label', { class: 'lab', for: 'langue-ecran', text: t('ecran.langue_ecran') }), h('div', { class: 'champ' }, langueEcran, ico('chevd'))),
    h('div', { class: 'f' }, h('label', { class: 'lab', for: 'langue-assistant', text: t('ecran.langue_assistant') }), h('div', { class: 'champ' }, langueAssistant, ico('chevd')),
      h('p', { class: 'aide', id: 'langue-assistant-aide', text: t('ecran.langue_assistant_aide') })),
    h('div', { class: 'f' }, h('span', { class: 'lab', text: t('ecran.theme') }), seg),
    h('div', { class: 'f' }, h('label', { class: 'lab', for: 'luminosite' }, t('ecran.luminosite'), valeur), lum));
}

/* ---------- Wi-Fi ---------- */
const signal = (n) => t(n >= 3 ? 'wifi.signal_bon' : n === 2 ? 'wifi.signal_moyen' : 'wifi.signal_faible');
function carteWifi() {
  const w = S.wifi;
  const panneau = h('div', { 'aria-live': 'polite' });
  const lecture = w ? h('div', { class: 'rang' }, ico('wifi'), h('div', {},
    h('b', { text: w.ssid ? t('wifi.ssid', { ssid: w.ssid }) : t('wifi.aucun_reseau') }),
    h('small', { text: [CAT.mots['wifi.etat_' + w.etat] ? t('wifi.etat_' + w.etat) : '', w.ssid && w.etat === 'connecte' ? signal(w.niveau) : ''].filter(Boolean).join(' · ') })))
    : retour('err', t('erreur.wifi_illisible'));
  const bouton = h('button', { type: 'button', class: 'btn sec', onclick: scanner, text: t('wifi.changer') });
  async function scanner() {
    bouton.disabled = true;
    panneau.replaceChildren(retour('info', t('wifi.recherche')));
    try {
      await api('POST', '/api/wifi/scan');
      let r = null;
      for (let i = 0; i < 20; i++) { await attendre(i ? 700 : 300); r = await api('GET', '/api/wifi/scan'); if (!r.en_cours) break; }
      if (!r || !r.reseaux.length) { panneau.replaceChildren(retour('info', t('wifi.aucun_trouve'))); return; }
      panneau.replaceChildren(h('p', { class: 'sec-titre', text: t('wifi.choisissez') }), h('ul', { class: 'resultats' }, r.reseaux.map((n) =>
        h('li', {}, h('button', { type: 'button', class: 'lieu', onclick: () => saisir(n) }, ico('wifi'),
          h('div', {}, h('b', { text: n.ssid }), h('small', { text: `${signal(n.niveau)} · ${t(n.securise ? 'wifi.protege' : 'wifi.ouvert')}` })))))));
    } catch (e) { panneau.replaceChildren(retour('err', msg(e))); } finally { bouton.disabled = false; }
  }
  function saisir(n) {
    const mdp = h('input', { id: 'wifi-mdp', type: 'password', autocomplete: 'off', placeholder: t('wifi.mdp_ph'), 'data-lpignore': 'true' });
    const go = h('button', { type: 'button', class: 'btn', text: t('wifi.connecter'), onclick: () => connecter(n, mdp.value, go) });
    panneau.replaceChildren(h('p', { class: 'sec-titre', text: t('wifi.ssid', { ssid: n.ssid }) }),
      n.securise ? h('div', { class: 'f' }, h('label', { class: 'lab', for: 'wifi-mdp', text: t('wifi.mdp') }), h('div', { class: 'champ' }, ico('key'), mdp)) : null, go);
    if (n.securise) mdp.focus();
  }
  async function connecter(n, mdp, go) {
    go.disabled = true;
    panneau.append(retour('info', t('wifi.connexion_encours')));
    try {
      const r = await api('PUT', '/api/wifi', { ssid: n.ssid, mot_de_passe: mdp });
      let etat = null;
      for (let i = 0; i < 17; i++) {
        await attendre(1500);
        try { etat = await api('GET', '/api/wifi'); } catch (e) {
          if (r && r.adresse_apres) { panneau.replaceChildren(retour('info', t('wifi.reconnectez', { adresse: r.adresse_apres }))); return; }
          throw e;
        }
        if (etat.etat !== 'connexion') break;
      }
      if (etat && etat.etat === 'connecte') { S.wifi = etat; majSousTitre(); remplacer('wifi', carteWifi()); toast(t('wifi.connecte_a', { ssid: etat.ssid })); } else {
        panneau.replaceChildren(retour('err', (etat && etat.message) || t('erreur.connexion')));
      }
    } catch (e) { panneau.replaceChildren(retour('err', msg(e))); }
  }
  return carte('wifi', 'wifi', t('wifi.titre'), lecture, bouton, panneau);
}

/* ---------- appareil : mise à jour, sauvegarde, session ---------- */
const NOMS_REGLAGES = { zone: 'reg.nom_zone', affichage: 'reg.nom_affichage', 'services.stt': 'svc.stt', 'services.llm': 'svc.llm', 'services.tts': 'svc.tts' };
function carteAppareil() {
  const sys = S.systeme, maj = S.maj;
  const infos = sys ? h('div', { class: 'rang' }, ico('tablet'), h('div', {}, h('b', { text: t('appareil.version', { version: sys.version }) }),
    h('small', { text: t(sys.validee ? 'appareil.validee' : 'appareil.attente') }))) : retour('err', t('erreur.appareil_illisible'));
  const apresMaj = sessionStorageLire('maj') && maj && maj.dernier_resultat
    ? retour(maj.dernier_resultat.issue === 'validee' ? 'ok' : 'err', maj.dernier_resultat.message) : null;
  if (apresMaj) {
    sessionStorageEcrire('maj', '');
    toast(maj.dernier_resultat.message, maj.dernier_resultat.issue !== 'validee');
  }
  const zoneMaj = h('div', { 'aria-live': 'polite' });
  const fichierMaj = h('input', { type: 'file', accept: '.bin,application/octet-stream', hidden: true, 'aria-label': t('maj.fichier') });
  const choisirMaj = h('button', { type: 'button', class: 'btn sec', onclick: () => fichierMaj.click() }, ico('up'), t('maj.choisir'));
  fichierMaj.addEventListener('change', () => {
    const f = fichierMaj.files[0];
    if (!f) return;
    const envoyer = h('button', { type: 'button', class: 'btn', text: t('maj.envoyer'), onclick: () => televerser(f, envoyer) });
    zoneMaj.replaceChildren(retour('info', t('maj.fichier_taille', { nom: f.name, ko: Math.max(1, Math.round(f.size / 1024)) })), envoyer);
  });
  function televerser(f, bouton) {
    bouton.disabled = true;
    const prog = h('progress', { max: '100', value: '0', 'aria-label': t('maj.envoi') });
    zoneMaj.append(prog);
    const xhr = new XMLHttpRequest();
    xhr.open('POST', '/api/update');
    xhr.setRequestHeader('Authorization', 'Bearer ' + jeton);
    xhr.setRequestHeader('Content-Type', 'application/octet-stream');
    xhr.upload.onprogress = (e) => { if (e.lengthComputable) prog.value = (e.loaded / e.total) * 100; };
    xhr.onerror = () => zoneMaj.replaceChildren(retour('err', t('erreur.injoignable')));
    xhr.onload = () => {
      let json = null;
      try { json = JSON.parse(xhr.responseText); } catch (e) { /* réponse non JSON */ }
      if (xhr.status === 200) { sessionStorageEcrire('maj', '1'); attendreRedemarrage(zoneMaj); return; }
      if (xhr.status === 401) { oublier(); montrerAppairage(); return; }
      zoneMaj.replaceChildren(retour('err', (json && json.erreur && json.erreur.message) || t('erreur.interne')));
    };
    xhr.send(f);
  }
  const retourArriere = maj && maj.precedente && maj.precedente.disponible ? (() => {
    const zone = h('div');
    const demande = h('button', { type: 'button', class: 'btn sec', text: t('maj.retour', { version: maj.precedente.version }), onclick: () => {
      zone.replaceChildren(h('p', { class: 'aide avert', text: t('maj.retour_confirm', { version: maj.precedente.version }) }),
        h('div', { class: 'btns' }, h('button', { type: 'button', class: 'btn sec', text: t('action.annuler'), onclick: () => zone.replaceChildren(demande) }),
          h('button', { type: 'button', class: 'btn', text: t('maj.revenir'), onclick: async () => {
            try { await api('POST', '/api/update/rollback'); sessionStorageEcrire('maj', '1'); attendreRedemarrage(zone); } catch (e) { zone.replaceChildren(retour('err', msg(e)), demande); }
          } })));
    } });
    zone.append(demande);
    return zone;
  })() : null;
  /* sauvegarde sans clés */
  const zoneImport = h('div', { 'aria-live': 'polite' });
  const exporter = h('button', { type: 'button', class: 'btn sec', onclick: async () => {
    try {
      const rep = await fetch('/api/settings/export', { headers: { Authorization: 'Bearer ' + jeton } });
      if (rep.status === 401) { oublier(); montrerAppairage(); return; }
      if (!rep.ok) throw new Erreur(t('erreur.interne'), rep.status);
      const url = URL.createObjectURL(await rep.blob());
      const a = h('a', { href: url, download: ((rep.headers.get('content-disposition') || '').match(/filename="([^"]+)"/) || [])[1] || 'reglages-ecran.json' });
      document.body.append(a); a.click(); a.remove();
      setTimeout(() => URL.revokeObjectURL(url), 2000);
      toast(t('reg.sauvegardes'));
    } catch (e) { toast(e instanceof Erreur ? e.message : t('erreur.injoignable'), true); }
  } }, ico('down'), t('reg.sauvegarder'));
  const fichierImport = h('input', { type: 'file', accept: '.json,application/json', hidden: true, 'aria-label': t('reg.fichier') });
  const importer = h('button', { type: 'button', class: 'btn sec', onclick: () => fichierImport.click() }, ico('up'), t('reg.restaurer'));
  fichierImport.addEventListener('change', async () => {
    const f = fichierImport.files[0];
    fichierImport.value = '';
    if (!f) return;
    if (f.size > 16384) { zoneImport.replaceChildren(retour('err', t('erreur.reglages_gros'))); return; }
    const texte = await f.text();
    try {
      const r = await api('POST', '/api/settings/import?simuler=1', texte);
      if (r.erreurs && r.erreurs.length) { zoneImport.replaceChildren(...r.erreurs.map((x) => retour('err', x.message || x))); return; }
      const quoi = (r.appliques || []).map((k) => (NOMS_REGLAGES[k] ? t(NOMS_REGLAGES[k]) : k)).join(', ') || t('reg.rien');
      const appliquer = h('button', { type: 'button', class: 'btn', text: t('action.appliquer'), onclick: async () => {
        try { await api('POST', '/api/settings/import', texte); toast(t('reg.restaures')); demarrer(); } catch (e) { zoneImport.replaceChildren(retour('err', msg(e))); }
      } });
      zoneImport.replaceChildren(...nets(retour('info', t('reg.changerait', { quoi })),
        ...(r.ignores || []).map((x) => h('p', { class: 'aide', text: x.message })),
        (r.cles_a_ressaisir || []).length ? h('p', { class: 'aide avert', text: t('reg.ressaisir', { services: r.cles_a_ressaisir.map((k) => (NOMS[k] ? t(NOMS[k][0]) : k)).join(', ') }) }) : null,
        h('div', { class: 'btns' }, h('button', { type: 'button', class: 'btn sec', text: t('action.annuler'), onclick: () => zoneImport.replaceChildren() }), appliquer)));
    } catch (e) { zoneImport.replaceChildren(retour('err', msg(e))); }
  });
  const fermer = h('button', { type: 'button', class: 'lien', text: t('session.fermer'), onclick: async () => { try { await api('DELETE', '/api/pair'); } catch (e) { /* déjà fermée */ } oublier(); montrerAppairage(); } });
  return carte('appareil', 'tablet', t('appareil.titre'), infos, apresMaj,
    h('p', { class: 'sec-titre', text: t('maj.titre') }), choisirMaj, fichierMaj, zoneMaj, retourArriere,
    h('p', { class: 'sec-titre', text: t('reg.titre') }), exporter, importer, fichierImport, zoneImport,
    h('p', { class: 'aide', text: t('reg.sans_cle_aide') }), fermer);
}
function sessionStorageLire(k) { try { return sessionStorage.getItem(k); } catch (e) { return ''; } }
function sessionStorageEcrire(k, val) { try { sessionStorage.setItem(k, val); } catch (e) { /* tant pis */ } }
async function attendreRedemarrage(zone) {
  zone.replaceChildren(retour('info', t('maj.redemarre')));
  await attendre(3000);
  for (let i = 0; i < 45; i++) {
    try {
      const st = await fetch('/api/status').then((r) => (r.ok ? r.json() : null));
      if (st) {
        if (st.langue && st.langue !== CAT.code) await appliquerLangue(st.langue);
        oublier(); montrerAppairage(t('appairage.redemarre')); return;
      }
    } catch (e) { /* encore éteint */ }
    await attendre(2000);
  }
  zone.replaceChildren(retour('err', t('erreur.ne_repond_plus')));
}

demarrer();
})();
