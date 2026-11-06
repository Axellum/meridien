'use strict';
/* Page de réglages (esquisse B, choisie par Axel le 06/11/2026). JS sans framework : le contrat est docs/api-reglages.md.
   Règles : une clé API est écrite mais jamais lue ; les erreurs s'affichent en phrases françaises, jamais en code. */
(() => {
const NBSP = ' ';
const SVGNS = 'http://www.w3.org/2000/svg';
const $ = (s, r = document) => r.querySelector(s);
const attendre = (ms) => new Promise((r) => setTimeout(r, ms));
const NOMS = { stt: ['Comprendre ma voix', 'mic'], llm: ['Réfléchir', 'chat'], tts: ['Parler', 'speaker'] };
const SECTIONS = [['ville', 'Météo'], ['assistant', 'Assistant'], ['ecran', 'Écran'], ['wifi', 'Wi-Fi'], ['appareil', 'Appareil']];
const S = { statut: null, systeme: null, zone: null, meteo: null, display: null, services: null, presets: null, wifi: null, maj: null, tests: {} };
let jeton = '';
try { jeton = sessionStorage.getItem('jeton') || ''; } catch (e) { /* stockage refusé : la page redemandera le code */ }

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
  const t = $('#toast');
  t.className = erreur ? 'err' : '';
  t.replaceChildren(ico(erreur ? 'cross' : 'check'), texte);
  t.hidden = false;
  clearTimeout(tToast);
  tToast = setTimeout(() => { t.hidden = true; }, erreur ? 5000 : 2500);
}
const carte = (id, icone, titre, ...contenu) =>
  h('section', { class: 'carte', id, 'aria-labelledby': 't-' + id }, h('h2', { id: 't-' + id }, ico(icone), titre), ...contenu);
const remplacer = (id, nouvelle) => { const a = document.getElementById(id); if (a) a.replaceWith(nouvelle); return nouvelle; };

/* ---------- accès à l'API ---------- */
class Erreur extends Error {
  constructor(message, statut, id, champ) { super(message); this.statut = statut; this.id = id; this.champ = champ; }
}
const msg = (e) => (e instanceof Erreur ? e.message : "L'écran a rencontré un problème. Réessayez.");
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
    throw new Erreur("L'écran ne répond pas. Est-il allumé et sur le même Wi-Fi ?", 0, 'injoignable');
  }
  if (rep.status === 204) return null;
  const json = (rep.headers.get('content-type') || '').includes('json') ? await rep.json().catch(() => null) : null;
  if (rep.ok) return json;
  if (rep.status === 401) { oublier(); montrerAppairage(); }
  const er = json && json.erreur;
  throw new Erreur((er && er.message) || "L'écran a rencontré un problème. Réessayez.", rep.status, er && er.id, er && er.champ);
}
function oublier() {
  jeton = '';
  try { sessionStorage.removeItem('jeton'); } catch (e) { /* sans importance */ }
}

/* ---------- appairage ---------- */
function montrerAppairage(message) {
  const retours = h('div');
  const champ = h('input', { id: 'code', inputmode: 'numeric', autocomplete: 'one-time-code', maxlength: '6', pattern: '[0-9]*', 'aria-label': "Code d'appairage" });
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
    h('span', { class: 'logo' }, ico('tablet')), h('h1', { text: 'Mon écran' }),
    h('p', { text: message || "Entrez le code à 6 chiffres affiché sur l'écran de l'appareil." }),
    champ, retours, h('button', { type: 'button', class: 'btn', onclick: envoyer, text: 'Continuer' })));
  champ.focus();
}

/* ---------- démarrage ---------- */
async function demarrer() {
  let st;
  try { st = await api('GET', '/api/status'); } catch (e) {
    $('#racine').replaceChildren(h('main', { class: 'appairage' }, h('h1', { text: 'Mon écran' }), retour('err', msg(e)),
      h('button', { type: 'button', class: 'btn', onclick: demarrer, text: 'Réessayer' })));
    return;
  }
  if (st.appairage !== 'ok') { oublier(); montrerAppairage(); return; }
  S.statut = st;
  const [sys, zone, disp, svc, pre, wifi, maj] = await Promise.allSettled(
    ['/api/system', '/api/location', '/api/display', '/api/services', '/api/presets', '/api/wifi', '/api/update'].map((c) => api('GET', c)));
  if (!jeton) return; /* une réponse 401 a déjà ramené à l'appairage */
  const val = (r) => (r.status === 'fulfilled' ? r.value : null);
  Object.assign(S, { systeme: val(sys), zone: val(zone), display: val(disp), services: val(svc), presets: val(pre), wifi: val(wifi), maj: val(maj) });
  S.meteo = null;
  construirePage();
  if (S.zone && S.zone.definie !== false) chargerMeteo(1);
}

function construirePage() {
  const nav = h('nav', { class: 'ancres', 'aria-label': 'Sections' },
    SECTIONS.map(([id, nom]) => h('a', { href: '#' + id, text: nom, 'aria-current': id === 'ville' ? 'true' : 'false' })));
  $('#racine').replaceChildren(h('div', { class: 'page' },
    h('header', { class: 'haut' }, h('div', { class: 'top' }, h('span', { class: 'logo' }, ico('tablet')),
      h('div', {}, h('b', { text: 'Mon écran' }), h('small', { id: 'sous-titre' }))), nav),
    h('main', {}, carteVille(), carteAssistant(), carteEcran(), carteWifi(), carteAppareil())));
  majSousTitre();
  const liens = [...nav.querySelectorAll('a')];
  const actif = (id) => liens.forEach((a) => a.setAttribute('aria-current', String(a.getAttribute('href') === '#' + id)));
  liens.forEach((a) => a.addEventListener('click', () => actif(a.getAttribute('href').slice(1))));
  if ('IntersectionObserver' in window) {
    const obs = new IntersectionObserver((es) => es.forEach((e) => { if (e.isIntersecting) actif(e.target.id); }), { rootMargin: '-25% 0px -65% 0px' });
    SECTIONS.forEach(([id]) => obs.observe(document.getElementById(id)));
  }
}
function majSousTitre() {
  const w = S.wifi, t = $('#sous-titre');
  if (!t) return;
  const nom = (S.statut && S.statut.nom) || 'écran';
  t.textContent = w && w.etat === 'connecte' && w.ssid ? `${nom} · connecté à « ${w.ssid} »` : nom;
}

/* ---------- ville et météo ---------- */
let majVille = () => {};
let horloge;
const heureDe = (fuseau) => {
  try {
    const p = new Intl.DateTimeFormat('fr-FR', { timeZone: fuseau, hour: 'numeric', minute: '2-digit', hourCycle: 'h23' }).formatToParts(new Date());
    const g = (t) => (p.find((x) => x.type === t) || {}).value;
    return `${g('hour')}${NBSP}h${NBSP}${g('minute')}`;
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
  const champ = h('input', { id: 'ville-q', type: 'search', placeholder: 'Tapez le nom d’une ville', autocomplete: 'off', autocapitalize: 'off', spellcheck: 'false', enterkeyhint: 'search' });
  majVille = () => {
    const z = S.zone, def = z && z.definie !== false;
    etiquette.textContent = def ? 'Changer de ville' : 'Choisir ma ville';
    if (!def) {
      hero.replaceChildren(h('div', { class: 'hero' }, h('div', {}, h('small', { text: 'Pas encore de ville' }), h('b', { class: 'ville', text: 'Choisissez-la' }))));
      aide.textContent = "L'écran en déduit la météo et l'heure.";
      return;
    }
    const m = S.meteo && S.meteo.disponible ? S.meteo.actuelle : null;
    hero.replaceChildren(h('div', { class: 'hero' },
      h('div', {}, h('small', { text: 'Météo à' }), h('b', { class: 'ville', text: z.nom }), h('small', { text: [z.region, z.pays].filter(Boolean).join(' · ') })),
      h('div', { class: 'temp' }, m ? [h('b', { text: `${Math.round(m.temperature)}${NBSP}°C` }), h('small', { text: m.libelle })]
        : h('small', { text: S.meteo ? 'Météo indisponible' : 'Météo en cours…' }))));
    aide.textContent = `Fuseau horaire${NBSP}: ${z.fuseau} · il est ${heureDe(z.fuseau)}`;
  };
  let minuteur, ctrl, seq = 0;
  const chercher = async (q) => {
    if (ctrl) ctrl.abort();
    ctrl = new AbortController();
    const mon = ++seq;
    retours.replaceChildren(retour('info', 'Recherche…'));
    try {
      const r = await api('GET', '/api/geocode?q=' + encodeURIComponent(q), undefined, ctrl.signal);
      if (mon !== seq) return;
      liste.replaceChildren(...r.resultats.map((l) => h('li', {}, h('button', { type: 'button', class: 'lieu', onclick: () => choisir(l) },
        ico('pin'), h('div', {}, h('b', { text: l.nom }), h('small', { text: [l.region, l.pays].filter(Boolean).join(' · ') }))))));
      liste.hidden = !r.resultats.length;
      retours.replaceChildren(r.resultats.length ? '' : retour('info', `Aucun lieu trouvé pour « ${q} ».`));
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
    retours.replaceChildren(q && q.length < 3 ? h('p', { class: 'aide', text: 'Tapez au moins 3 lettres.' }) : '');
    if (q.length >= 3) minuteur = setTimeout(() => chercher(q), 350);
  });
  const choisir = async (lieu) => {
    liste.querySelectorAll('button').forEach((b) => { b.disabled = true; });
    try {
      S.zone = await api('PUT', '/api/location', lieu);
      champ.value = ''; liste.replaceChildren(); liste.hidden = true; retours.replaceChildren();
      S.meteo = null; majVille();
      toast('Ville enregistrée' + NBSP + ': ' + S.zone.nom);
      chargerMeteo(1);
    } catch (e) {
      liste.querySelectorAll('button').forEach((b) => { b.disabled = false; });
      retours.replaceChildren(retour('err', msg(e)));
    }
  };
  clearInterval(horloge);
  horloge = setInterval(majVille, 30000);
  majVille();
  return carte('ville', 'pin', 'Ma ville', hero, aide, h('div', { class: 'f' }, etiquette, h('div', { class: 'champ' }, ico('search'), champ)), retours, liste);
}

/* ---------- assistant : trois services ---------- */
function carteAssistant() {
  const blocs = ['stt', 'llm', 'tts'].map(blocService);
  blocs.forEach((b) => b.entete.addEventListener('click', () => blocs.forEach((x) => x.ouvrir(x === b && !x.estOuvert()))));
  return carte('assistant', 'mic', 'Assistant vocal',
    h('p', { class: 'aide', text: 'Trois services : comprendre votre voix, réfléchir, parler. Touchez-en un pour le régler.' }),
    ...blocs.flatMap((b) => [b.entete, b.detail]));
}
function blocService(id) {
  const [titre, icone] = NOMS[id];
  const presets = (S.presets && S.presets[id]) || [];
  let svc = (S.services && S.services[id]) || { configure: false, cle_definie: false };
  const v = { preset: svc.preset || '', url: svc.url || '', modele: svc.modele || '', voix: svc.voix || '', langue: svc.langue || '', cle: '', remplacerCle: false, effacerCle: false };
  let modifie = false;
  const sous = h('small');
  const pastille = h('span', { class: 'pill' });
  const entete = h('button', { type: 'button', class: 'rang', 'aria-expanded': 'false', 'aria-controls': 'det-' + id },
    ico(icone), h('div', {}, h('b', { text: titre }), sous), pastille, ico('chev'));
  const detail = h('div', { class: 'detail', id: 'det-' + id, hidden: true });
  const nomPreset = (p) => { const x = presets.find((q) => q.id === p); return x ? x.nom : 'Personnalisé'; };
  const majEntete = () => {
    const t = S.tests[id];
    sous.textContent = svc.configure ? `${nomPreset(svc.preset)} · ${svc.cle_definie ? 'clé enregistrée' : 'sans clé'}` : 'Pas encore réglé';
    pastille.className = 'pill' + (t ? (t.ok ? ' ok' : ' err') : '');
    pastille.textContent = t ? (t.ok ? 'Prêt' : 'À vérifier') : (svc.configure ? 'Réglé' : 'À régler');
  };
  /* formulaire */
  const champs = {};
  const rangee = (cle, label, attrs, valeur) => {
    const input = h('input', { id: `${id}-${cle}`, autocomplete: 'off', autocapitalize: 'off', spellcheck: 'false', ...attrs, value: valeur });
    input.addEventListener('input', () => { v[cle] = input.value; input.removeAttribute('aria-invalid'); touche(); });
    champs[cle] = input;
    return h('div', { class: 'f' }, h('label', { class: 'lab', for: input.id, text: label }), h('div', { class: 'champ' }, input));
  };
  const chips = h('div', { class: 'chips', role: 'radiogroup', 'aria-label': 'Où tourne-t-il ?' });
  const aidePreset = h('p', { class: 'aide' });
  const dessinerChips = () => {
    chips.replaceChildren(...presets.map((p) => h('button', { type: 'button', class: 'chip', role: 'radio', 'aria-checked': String(v.preset === p.id), onclick: () => choisirPreset(p) },
      v.preset === p.id ? ico('check') : null, p.nom)));
    const p = presets.find((x) => x.id === v.preset);
    aidePreset.replaceChildren(p ? [p.aide || '', p.local ? " Remplacez « localhost » par l'adresse de votre PC, par exemple 192.168.0.20." : ''].join('').trim() : '');
    aidePreset.hidden = !aidePreset.textContent;
  };
  const choisirPreset = (p) => {
    v.preset = p.id;
    if (p.url) { v.url = p.url; champs.url.value = p.url; }
    champs.url.removeAttribute('aria-invalid');
    dessinerChips(); dessinerCle(); touche();
  };
  const champUrl = rangee('url', 'Adresse du service', { type: 'url', inputmode: 'url', placeholder: 'https://…' }, v.url);
  const datalist = h('datalist', { id: `mod-${id}` });
  const champModele = rangee('modele', 'Modèle', { list: `mod-${id}`, placeholder: 'Choisir ou saisir un modèle' }, v.modele);
  const champsExtra = id === 'tts' ? rangee('voix', 'Voix', { placeholder: 'Facultatif' }, v.voix) : id === 'stt' ? rangee('langue', 'Langue parlée', { placeholder: 'fr' }, v.langue) : null;
  /* clé : écriture seule */
  const cleEl = h('div', { class: 'f' });
  const dessinerCle = () => {
    const p = presets.find((x) => x.id === v.preset);
    const lab = h('span', { class: 'lab', text: "Clé d'accès" });
    let corps;
    if (svc.cle_definie && !v.remplacerCle && !v.effacerCle) {
      corps = h('div', {}, h('div', { class: 'champ cle' }, ico('key'), h('span', { class: 'val', text: 'Clé enregistrée' })),
        h('div', { class: 'liens' },
          h('button', { type: 'button', class: 'lien', onclick: () => { v.remplacerCle = true; dessinerCle(); champs.cle.focus(); }, text: 'Remplacer' }),
          h('button', { type: 'button', class: 'lien', onclick: () => { v.effacerCle = true; dessinerCle(); touche(); }, text: 'Effacer' })));
    } else if (v.effacerCle) {
      corps = h('div', {}, retour('info', 'La clé sera effacée quand vous enregistrerez.'),
        h('button', { type: 'button', class: 'lien', onclick: () => { v.effacerCle = false; dessinerCle(); touche(); }, text: 'Annuler' }));
    } else {
      const input = h('input', { id: `${id}-cle`, type: 'password', autocomplete: 'off', autocapitalize: 'off', spellcheck: 'false', 'data-lpignore': 'true',
        placeholder: p && p.cle_requise === false ? 'Facultatif pour ce service' : 'Collez la clé ici', value: v.cle });
      input.addEventListener('input', () => { v.cle = input.value; input.removeAttribute('aria-invalid'); touche(); });
      champs.cle = input;
      corps = h('div', {}, h('div', { class: 'champ' }, ico('key'), input),
        svc.cle_definie ? h('button', { type: 'button', class: 'lien', onclick: () => { v.remplacerCle = false; v.cle = ''; dessinerCle(); touche(); }, text: 'Garder la clé enregistrée' }) : null);
    }
    cleEl.replaceChildren(lab, corps, h('p', { class: 'aide', text: "Une fois enregistrée, la clé ne s'affiche plus nulle part." }));
  };
  /* actions */
  const resTest = h('div', { 'aria-live': 'polite' });
  const resSave = h('div', { 'aria-live': 'polite' });
  const btnTest = h('button', { type: 'button', class: 'btn sec', onclick: () => tester() }, ico('play'), 'Tester');
  const btnSave = h('button', { type: 'button', class: 'btn', onclick: () => enregistrer(), text: 'Enregistrer' });
  const touche = () => { modifie = true; majBoutons(); };
  const majBoutons = () => { btnSave.disabled = !v.url.trim() || !(modifie || !svc.configure); };
  const corpsService = () => {
    const c = { url: v.url, modele: v.modele };
    if (id === 'tts') c.voix = v.voix;
    if (id === 'stt') c.langue = v.langue;
    return c;
  };
  async function tester() {
    btnTest.disabled = true;
    resTest.replaceChildren(retour('info', 'Test en cours…'));
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
      if (!r || r.etat !== 'termine') throw new Erreur('Le test prend trop de temps. Réessayez.', 0, 'trop_long');
      S.tests[id] = r;
      const n = (r.modeles || []).length;
      if (n) datalist.replaceChildren(...r.modeles.map((m) => h('option', { value: m })));
      resTest.replaceChildren(...nets(retour(r.ok ? 'ok' : 'err', r.message),
        r.ok && n ? h('p', { class: 'aide', text: `${n} modèle${n > 1 ? 's' : ''} trouvé${n > 1 ? 's' : ''}${v.modele ? '.' : ` : choisissez-en un.`}` }) : null));
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
      toast('Enregistré');
    } catch (e) {
      resSave.replaceChildren(retour('err', msg(e)));
      if (e.champ && champs[e.champ]) champs[e.champ].setAttribute('aria-invalid', 'true');
      majBoutons();
    }
  }
  detail.append(...nets(h('div', { class: 'f' }, h('span', { class: 'lab', text: 'Où tourne-t-il ?' }), chips, aidePreset),
    champUrl, champModele, datalist, champsExtra, cleEl, h('div', { class: 'btns' }, btnTest, btnSave), resTest, resSave));
  dessinerChips(); dessinerCle(); majEntete(); majBoutons();
  const ouvrir = (oui) => {
    entete.setAttribute('aria-expanded', String(oui));
    detail.hidden = !oui;
    entete.lastChild.replaceWith(ico(oui ? 'chevd' : 'chev'));
  };
  return { entete, detail, ouvrir, estOuvert: () => !detail.hidden };
}

/* ---------- écran ---------- */
function carteEcran() {
  const d = S.display;
  if (!d) return carte('ecran', 'sun', 'Écran', retour('err', 'Les réglages de l’écran ne sont pas lisibles pour le moment.'));
  const enregistrer = async (champ) => {
    try { S.display = await api('PUT', '/api/display', champ); toast('Enregistré'); } catch (e) { toast(msg(e), true); remplacer('ecran', carteEcran()); }
  };
  const langue = h('select', { id: 'langue', onchange: () => enregistrer({ langue: langue.value }) },
    (d.langues || []).map((l) => h('option', { value: l.code, text: l.nom, selected: l.code === d.langue })));
  const THEMES = { auto: 'Auto', clair: 'Clair', sombre: 'Sombre' };
  const seg = h('div', { class: 'seg', role: 'radiogroup', 'aria-label': 'Thème' }, (d.themes || Object.keys(THEMES)).map((t) =>
    h('button', { type: 'button', role: 'radio', 'aria-checked': String(t === d.theme), text: THEMES[t] || t, onclick: () => {
      seg.querySelectorAll('button').forEach((b) => b.setAttribute('aria-checked', String(b === seg.querySelector(`[data-t="${t}"]`))));
      enregistrer({ theme: t });
    }, 'data-t': t })));
  const valeur = h('span', { class: 'valeur', text: `${d.luminosite}${NBSP}%` });
  const lum = h('input', { id: 'luminosite', type: 'range', min: '5', max: '100', step: '1', value: String(d.luminosite), 'aria-valuetext': `${d.luminosite} %` });
  lum.addEventListener('input', () => { valeur.textContent = `${lum.value}${NBSP}%`; lum.setAttribute('aria-valuetext', `${lum.value} %`); });
  lum.addEventListener('change', () => enregistrer({ luminosite: Number(lum.value) }));
  return carte('ecran', 'sun', 'Écran',
    h('div', { class: 'f' }, h('label', { class: 'lab', for: 'langue', text: 'Langue' }), h('div', { class: 'champ' }, langue, ico('chevd'))),
    h('div', { class: 'f' }, h('span', { class: 'lab', text: 'Thème' }), seg),
    h('div', { class: 'f' }, h('label', { class: 'lab', for: 'luminosite' }, 'Luminosité', valeur), lum));
}

/* ---------- Wi-Fi ---------- */
const ETATS_WIFI = { connecte: 'Connecté', connexion: 'Connexion en cours…', echec: 'Échec de la connexion', deconnecte: 'Non connecté', point_d_acces: 'Mise en service' };
const signal = (n) => (n >= 3 ? 'signal bon' : n === 2 ? 'signal moyen' : 'signal faible');
function carteWifi() {
  const w = S.wifi;
  const panneau = h('div', { 'aria-live': 'polite' });
  const lecture = w ? h('div', { class: 'rang' }, ico('wifi'), h('div', {},
    h('b', { text: w.ssid ? `« ${w.ssid} »` : 'Aucun réseau' }),
    h('small', { text: [ETATS_WIFI[w.etat] || '', w.ssid && w.etat === 'connecte' ? signal(w.niveau) : ''].filter(Boolean).join(' · ') })))
    : retour('err', 'Le Wi-Fi n’est pas lisible pour le moment.');
  const bouton = h('button', { type: 'button', class: 'btn sec', onclick: scanner, text: 'Changer de réseau' });
  async function scanner() {
    bouton.disabled = true;
    panneau.replaceChildren(retour('info', 'Recherche des réseaux…'));
    try {
      await api('POST', '/api/wifi/scan');
      let r = null;
      for (let i = 0; i < 20; i++) { await attendre(i ? 700 : 300); r = await api('GET', '/api/wifi/scan'); if (!r.en_cours) break; }
      if (!r || !r.reseaux.length) { panneau.replaceChildren(retour('info', 'Aucun réseau trouvé.')); return; }
      panneau.replaceChildren(h('p', { class: 'sec-titre', text: 'Choisissez votre réseau' }), h('ul', { class: 'resultats' }, r.reseaux.map((n) =>
        h('li', {}, h('button', { type: 'button', class: 'lieu', onclick: () => saisir(n) }, ico('wifi'),
          h('div', {}, h('b', { text: n.ssid }), h('small', { text: `${signal(n.niveau)} · ${n.securise ? 'protégé' : 'ouvert'}` })))))));
    } catch (e) { panneau.replaceChildren(retour('err', msg(e))); } finally { bouton.disabled = false; }
  }
  function saisir(n) {
    const mdp = h('input', { id: 'wifi-mdp', type: 'password', autocomplete: 'off', placeholder: 'Mot de passe du Wi-Fi', 'data-lpignore': 'true' });
    const go = h('button', { type: 'button', class: 'btn', text: 'Se connecter', onclick: () => connecter(n, mdp.value, go) });
    panneau.replaceChildren(h('p', { class: 'sec-titre', text: `« ${n.ssid} »` }),
      n.securise ? h('div', { class: 'f' }, h('label', { class: 'lab', for: 'wifi-mdp', text: 'Mot de passe' }), h('div', { class: 'champ' }, ico('key'), mdp)) : null, go);
    if (n.securise) mdp.focus();
  }
  async function connecter(n, mdp, go) {
    go.disabled = true;
    panneau.append(retour('info', 'Connexion en cours…'));
    try {
      const r = await api('PUT', '/api/wifi', { ssid: n.ssid, mot_de_passe: mdp });
      let etat = null;
      for (let i = 0; i < 17; i++) {
        await attendre(1500);
        try { etat = await api('GET', '/api/wifi'); } catch (e) {
          if (r && r.adresse_apres) { panneau.replaceChildren(retour('info', `Reconnectez votre téléphone à votre Wi-Fi, puis ouvrez ${r.adresse_apres}.`)); return; }
          throw e;
        }
        if (etat.etat !== 'connexion') break;
      }
      if (etat && etat.etat === 'connecte') { S.wifi = etat; majSousTitre(); remplacer('wifi', carteWifi()); toast('Connecté à « ' + etat.ssid + ' »'); } else {
        panneau.replaceChildren(retour('err', (etat && etat.message) || 'La connexion a échoué. Réessayez.'));
      }
    } catch (e) { panneau.replaceChildren(retour('err', msg(e))); }
  }
  return carte('wifi', 'wifi', 'Wi-Fi', lecture, bouton, panneau);
}

/* ---------- appareil : mise à jour, sauvegarde, session ---------- */
const NOMS_REGLAGES = { zone: 'Ville', affichage: 'Écran', 'services.stt': 'Comprendre ma voix', 'services.llm': 'Réfléchir', 'services.tts': 'Parler' };
function carteAppareil() {
  const sys = S.systeme, maj = S.maj;
  const infos = sys ? h('div', { class: 'rang' }, ico('tablet'), h('div', {}, h('b', { text: `Version ${sys.version}` }),
    h('small', { text: sys.validee ? 'Version validée' : 'En attente de validation' }))) : retour('err', 'Les informations de l’appareil ne sont pas lisibles.');
  const apresMaj = sessionStorageLire('maj') && maj && maj.dernier_resultat
    ? retour(maj.dernier_resultat.issue === 'validee' ? 'ok' : 'err', maj.dernier_resultat.message) : null;
  if (apresMaj) {
    sessionStorageEcrire('maj', '');
    toast(maj.dernier_resultat.message, maj.dernier_resultat.issue !== 'validee');
  }
  const zoneMaj = h('div', { 'aria-live': 'polite' });
  const fichierMaj = h('input', { type: 'file', accept: '.bin,application/octet-stream', hidden: true, 'aria-label': 'Fichier de mise à jour' });
  const choisirMaj = h('button', { type: 'button', class: 'btn sec', onclick: () => fichierMaj.click() }, ico('up'), 'Mettre à jour l’écran');
  fichierMaj.addEventListener('change', () => {
    const f = fichierMaj.files[0];
    if (!f) return;
    const envoyer = h('button', { type: 'button', class: 'btn', text: 'Envoyer à l’écran', onclick: () => televerser(f, envoyer) });
    zoneMaj.replaceChildren(retour('info', `${f.name} (${Math.max(1, Math.round(f.size / 1024))}${NBSP}Ko)`), envoyer);
  });
  function televerser(f, bouton) {
    bouton.disabled = true;
    const prog = h('progress', { max: '100', value: '0', 'aria-label': 'Envoi en cours' });
    zoneMaj.append(prog);
    const xhr = new XMLHttpRequest();
    xhr.open('POST', '/api/update');
    xhr.setRequestHeader('Authorization', 'Bearer ' + jeton);
    xhr.setRequestHeader('Content-Type', 'application/octet-stream');
    xhr.upload.onprogress = (e) => { if (e.lengthComputable) prog.value = (e.loaded / e.total) * 100; };
    xhr.onerror = () => zoneMaj.replaceChildren(retour('err', "L'écran ne répond pas. Est-il allumé et sur le même Wi-Fi ?"));
    xhr.onload = () => {
      let json = null;
      try { json = JSON.parse(xhr.responseText); } catch (e) { /* réponse non JSON */ }
      if (xhr.status === 200) { sessionStorageEcrire('maj', '1'); attendreRedemarrage(zoneMaj); return; }
      if (xhr.status === 401) { oublier(); montrerAppairage(); return; }
      zoneMaj.replaceChildren(retour('err', (json && json.erreur && json.erreur.message) || "L'écran a rencontré un problème. Réessayez."));
    };
    xhr.send(f);
  }
  const retourArriere = maj && maj.precedente && maj.precedente.disponible ? (() => {
    const zone = h('div');
    const demande = h('button', { type: 'button', class: 'btn sec', text: `Revenir à la version ${maj.precedente.version}`, onclick: () => {
      zone.replaceChildren(h('p', { class: 'aide avert', text: `L'écran va redémarrer avec la version ${maj.precedente.version}.` }),
        h('div', { class: 'btns' }, h('button', { type: 'button', class: 'btn sec', text: 'Annuler', onclick: () => zone.replaceChildren(demande) }),
          h('button', { type: 'button', class: 'btn', text: 'Revenir', onclick: async () => {
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
      if (!rep.ok) throw new Erreur("L'écran a rencontré un problème. Réessayez.", rep.status);
      const url = URL.createObjectURL(await rep.blob());
      const a = h('a', { href: url, download: ((rep.headers.get('content-disposition') || '').match(/filename="([^"]+)"/) || [])[1] || 'reglages-ecran.json' });
      document.body.append(a); a.click(); a.remove();
      setTimeout(() => URL.revokeObjectURL(url), 2000);
      toast('Réglages sauvegardés (sans les clés)');
    } catch (e) { toast(e instanceof Erreur ? e.message : "L'écran ne répond pas. Est-il allumé et sur le même Wi-Fi ?", true); }
  } }, ico('down'), 'Sauvegarder mes réglages');
  const fichierImport = h('input', { type: 'file', accept: '.json,application/json', hidden: true, 'aria-label': 'Fichier de réglages' });
  const importer = h('button', { type: 'button', class: 'btn sec', onclick: () => fichierImport.click() }, ico('up'), 'Restaurer des réglages');
  fichierImport.addEventListener('change', async () => {
    const f = fichierImport.files[0];
    fichierImport.value = '';
    if (!f) return;
    if (f.size > 16384) { zoneImport.replaceChildren(retour('err', 'Ce fichier est trop gros pour être une sauvegarde de réglages.')); return; }
    const texte = await f.text();
    try {
      const r = await api('POST', '/api/settings/import?simuler=1', texte);
      if (r.erreurs && r.erreurs.length) { zoneImport.replaceChildren(...r.erreurs.map((x) => retour('err', x.message || x))); return; }
      const quoi = (r.appliques || []).map((k) => NOMS_REGLAGES[k] || k).join(', ') || 'rien';
      const appliquer = h('button', { type: 'button', class: 'btn', text: 'Appliquer', onclick: async () => {
        try { await api('POST', '/api/settings/import', texte); toast('Réglages restaurés'); demarrer(); } catch (e) { zoneImport.replaceChildren(retour('err', msg(e))); }
      } });
      zoneImport.replaceChildren(...nets(retour('info', `Ce fichier changerait${NBSP}: ${quoi}.`),
        ...(r.ignores || []).map((x) => h('p', { class: 'aide', text: x.message })),
        (r.cles_a_ressaisir || []).length ? h('p', { class: 'aide avert', text: `Pensez à ressaisir la clé de${NBSP}: ${r.cles_a_ressaisir.map((k) => (NOMS[k] || [k])[0]).join(', ')}.` }) : null,
        h('div', { class: 'btns' }, h('button', { type: 'button', class: 'btn sec', text: 'Annuler', onclick: () => zoneImport.replaceChildren() }), appliquer)));
    } catch (e) { zoneImport.replaceChildren(retour('err', msg(e))); }
  });
  const fermer = h('button', { type: 'button', class: 'lien', text: 'Fermer la session', onclick: async () => { try { await api('DELETE', '/api/pair'); } catch (e) { /* déjà fermée */ } oublier(); montrerAppairage(); } });
  return carte('appareil', 'tablet', 'Appareil', infos, apresMaj,
    h('p', { class: 'sec-titre', text: 'Mise à jour' }), choisirMaj, fichierMaj, zoneMaj, retourArriere,
    h('p', { class: 'sec-titre', text: 'Mes réglages' }), exporter, importer, fichierImport, zoneImport,
    h('p', { class: 'aide', text: 'La sauvegarde ne contient ni clé d’accès ni mot de passe.' }), fermer);
}
function sessionStorageLire(k) { try { return sessionStorage.getItem(k); } catch (e) { return ''; } }
function sessionStorageEcrire(k, val) { try { sessionStorage.setItem(k, val); } catch (e) { /* tant pis */ } }
async function attendreRedemarrage(zone) {
  zone.replaceChildren(retour('info', "L'écran redémarre. Cette page se reconnecte toute seule…"));
  await attendre(3000);
  for (let i = 0; i < 45; i++) {
    try {
      const st = await fetch('/api/status').then((r) => (r.ok ? r.json() : null));
      if (st) { oublier(); montrerAppairage("L'écran a redémarré. Entrez de nouveau le code affiché sur son écran."); return; }
    } catch (e) { /* encore éteint */ }
    await attendre(2000);
  }
  zone.replaceChildren(retour('err', "L'écran ne répond toujours pas. Vérifiez qu'il est allumé."));
}

demarrer();
})();
