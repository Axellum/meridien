#!/usr/bin/env node
// Faux appareil : implémente docs/api-reglages.md sur le PC pour développer et tester la page (web/app/).
// Rien ici n'est le firmware : l'état est en mémoire et disparaît à l'arrêt. Sans dépendance (Node >= 20).
//
// Ce qui est RÉEL : la recherche de lieu (proxy vers geocoding-api.open-meteo.com), la météo (api.open-meteo.com),
// les noms de fuseaux et leurs chaînes POSIX (lus dans components/core/include/ecran/fuseaux_donnees.inc, la table
// du firmware) et les libellés météo (lus dans meteo_wmo.hpp). Le reste (Wi-Fi, mise à jour, redémarrage) est simulé.
//
// Usage : node web/faux-serveur/serveur.mjs
//   PORT=8080 (page + API), PORT_SERVICE=PORT+1 (faux service compatible OpenAI pour tester la carte « Assistant »)
//   CODE_APPAIRAGE=123456, VERROU_S=60, CLE_MOCK=cle-de-test-123, REBOOT_MS=4000
// Écoute sur 127.0.0.1 seulement.
import http from 'node:http';
import { readFileSync } from 'node:fs';
import { readFile } from 'node:fs/promises';
import { randomBytes, randomInt, timingSafeEqual } from 'node:crypto';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import zlib from 'node:zlib';

const ICI = path.dirname(fileURLToPath(import.meta.url));
const APP = path.join(ICI, '..', 'app');
const NOYAU = path.join(ICI, '..', '..', 'components', 'core', 'include', 'ecran');
const PORT = Number(process.env.PORT || 8080);
const PORT_SERVICE = Number(process.env.PORT_SERVICE || PORT + 1);
const CODE = process.env.CODE_APPAIRAGE || String(randomInt(100000, 1000000));
const VERROU_S = Number(process.env.VERROU_S || 60);
const CLE_MOCK = process.env.CLE_MOCK || 'cle-de-test-123';
const REBOOT_MS = Number(process.env.REBOOT_MS || 4000);
const LIMITE_JSON = 4096, LIMITE_IMPORT = 16384, TAILLE_PARTITION = 4 * 1024 * 1024;

/* ---------- les tables du noyau pur (même source que le firmware) ---------- */
const FUSEAUX = new Map([...readFileSync(path.join(NOYAU, 'fuseaux_donnees.inc'), 'utf8').matchAll(/^\{"([^"]+)", "([^"]+)"\}/gm)].map((m) => [m[1], m[2]]));
const WMO = new Map([...readFileSync(path.join(NOYAU, 'meteo_wmo.hpp'), 'utf8').matchAll(/case (\d+): return \{"([^"]+)"/g)].map((m) => [Number(m[1]), m[2]]));
if (FUSEAUX.size < 400 || WMO.size !== 29) throw new Error('Tables du noyau illisibles : ' + FUSEAUX.size + ' fuseaux, ' + WMO.size + ' codes météo');

/* ---------- état de l'appareil (en mémoire) ---------- */
const etat = {
  nom: 'ecran-8e87', version: '0.1.0', precedente: '0.0.9', demarre: Date.now(),
  zone: null, display: { langue: 'fr', theme: 'auto', luminosite: 80 },
  services: { stt: null, llm: null, tts: null },
  wifi: { etat: 'connecte', ssid: 'Maison', niveau: 3, force: -61, ip: '192.168.0.52' },
  maj: { dernier: null }, redemarreJusqua: 0, meteo: null, scanFin: 0,
};
const secrets = { stt: '', llm: '', tts: '', wifi: 'ok' }; // jamais renvoyés par l'API
const tests = {};
const sessions = new Map();
let echecsCode = 0, verrouJusqua = 0;

const LANGUES = [{ code: 'fr', nom: 'Français' }, { code: 'en', nom: 'English' }];
const THEMES = ['auto', 'clair', 'sombre'];
const RESEAUX = [
  { ssid: 'Maison', niveau: 4, force: -55, securise: true },
  { ssid: 'Café Libre', niveau: 3, force: -66, securise: false },
  { ssid: 'Voisin_5G', niveau: 2, force: -74, securise: true },
];
const P = (id, nom, url, o = {}) => ({ id, nom, url, local: !!o.local, cle_requise: o.cle !== false, modele: '', aide: o.aide || '' });
const PRESETS = {
  stt: [P('openai', 'OpenAI', 'https://api.openai.com/v1', { aide: 'Service sur Internet : il faut une clé.' }),
    P('ollama', 'Ollama (mon PC)', 'http://localhost:11434/v1', { local: true, cle: false, aide: 'Ollama doit tourner sur votre PC.' }),
    P('personnalise', 'Autre', '', { cle: false, aide: 'Tout service compatible OpenAI.' })],
  llm: [P('openai', 'OpenAI', 'https://api.openai.com/v1', { aide: 'Service sur Internet : il faut une clé.' }),
    P('google', 'Google', 'https://generativelanguage.googleapis.com/v1beta/openai', { aide: 'Service sur Internet : il faut une clé.' }),
    P('lmstudio', 'LM Studio (mon PC)', 'http://localhost:1234/v1', { local: true, cle: false, aide: 'Lancez le serveur local de LM Studio sur le PC.' }),
    P('ollama', 'Ollama (mon PC)', 'http://localhost:11434/v1', { local: true, cle: false, aide: 'Ollama doit tourner sur votre PC.' }),
    P('personnalise', 'Autre', '', { cle: false, aide: 'Tout service compatible OpenAI.' })],
  tts: [P('openai', 'OpenAI', 'https://api.openai.com/v1', { aide: 'Service sur Internet : il faut une clé.' }),
    P('ollama', 'Ollama (mon PC)', 'http://localhost:11434/v1', { local: true, cle: false, aide: 'Ollama doit tourner sur votre PC.' }),
    P('kokoro', 'Kokoro (mon PC)', 'http://localhost:8880/v1', { local: true, cle: false, aide: 'Voix française locale, à lancer sur le PC.' }),
    P('personnalise', 'Autre', '', { cle: false, aide: 'Tout service compatible OpenAI.' })],
};

/* ---------- utilitaires HTTP ---------- */
class Refus extends Error {
  constructor(statut, id, message, champ, extra) { super(message); Object.assign(this, { statut, id, champ, extra }); }
}
const REPONSES_JSON = { 'Content-Type': 'application/json; charset=utf-8', 'Cache-Control': 'no-store' };
function json(res, statut, corps, extra = {}) {
  const b = JSON.stringify(corps);
  res.writeHead(statut, { ...REPONSES_JSON, ...extra });
  res.end(b);
}
async function lire(req, max) {
  const morceaux = [];
  let n = 0;
  for await (const m of req) {
    n += m.length;
    if (n > max) throw new Refus(413, 'trop_gros', "Ce fichier est trop gros pour l'appareil.");
    morceaux.push(m);
  }
  return Buffer.concat(morceaux);
}
async function lireJson(req, max = LIMITE_JSON, facultatif = false) {
  const b = await lire(req, max);
  if (!b.length && facultatif) return {};
  try { const j = JSON.parse(b.toString('utf8')); if (j && typeof j === 'object' && !Array.isArray(j)) return j; } catch (e) { /* illisible */ }
  throw new Refus(400, 'illisible', 'Cette demande n’est pas lisible.');
}
const egal = (a, b) => { const x = Buffer.from(String(a)), y = Buffer.from(String(b)); return x.length === y.length && timingSafeEqual(x, y); };
const estConnecte = (req) => sessions.has((req.headers.authorization || '').replace(/^Bearer /, ''));

/* ---------- validations (communes aux PUT et à l'import) ---------- */
function validerZone(z) {
  if (!z || typeof z !== 'object') throw new Refus(422, 'zone_invalide', 'Cette ville n’est pas valide.', 'nom');
  const nom = String(z.nom || '').trim();
  if (!nom || nom.length > 80) throw new Refus(422, 'nom_invalide', 'Le nom de la ville n’est pas valide.', 'nom');
  const lat = Number(z.latitude), lon = Number(z.longitude);
  if (!Number.isFinite(lat) || !Number.isFinite(lon) || Math.abs(lat) > 90 || Math.abs(lon) > 180) {
    throw new Refus(422, 'coordonnees_invalides', 'Ces coordonnées ne sont pas valides.', 'latitude');
  }
  const posix = FUSEAUX.get(String(z.fuseau || ''));
  if (!posix) throw new Refus(422, 'fuseau_inconnu', 'Ce fuseau horaire n’est pas connu de l’appareil.', 'fuseau');
  const txt = (x) => String(x || '').slice(0, 80);
  const zone = { nom, region: txt(z.region), pays: txt(z.pays), code_pays: txt(z.code_pays).slice(0, 3), latitude: lat, longitude: lon, fuseau: z.fuseau, posix };
  if (Number.isFinite(Number(z.altitude)) && z.altitude !== null && z.altitude !== '') zone.altitude = Number(z.altitude);
  zone.libelle = [zone.nom, zone.region, zone.pays].filter(Boolean).join(', ');
  return zone;
}
function validerAffichage(a) {
  const r = {};
  if (a.langue !== undefined) {
    if (!LANGUES.some((l) => l.code === a.langue)) throw new Refus(422, 'langue_inconnue', 'Cette langue n’est pas disponible.', 'langue');
    r.langue = a.langue;
  }
  if (a.theme !== undefined) {
    if (!THEMES.includes(a.theme)) throw new Refus(422, 'theme_inconnu', 'Ce thème n’existe pas.', 'theme');
    r.theme = a.theme;
  }
  if (a.luminosite !== undefined) {
    if (!Number.isInteger(a.luminosite) || a.luminosite < 5 || a.luminosite > 100) throw new Refus(422, 'luminosite', 'La luminosité doit être entre 5 et 100 %.', 'luminosite');
    r.luminosite = a.luminosite;
  }
  return r;
}
function validerUrl(u) {
  let url;
  try { url = new URL(String(u)); } catch (e) { throw new Refus(422, 'url_invalide', 'Cette adresse n’est pas valide.', 'url'); }
  if (!['http:', 'https:'].includes(url.protocol)) throw new Refus(422, 'url_protocole', 'L’adresse doit commencer par http:// ou https://.', 'url');
  if (url.username || url.password) throw new Refus(422, 'url_identifiant', 'Retirez l’identifiant de l’adresse : la clé se saisit à part.', 'url');
  const s = String(u).trim().replace(/\/+$/, '');
  if (s.length > 200) throw new Refus(422, 'url_longue', 'Cette adresse est trop longue.', 'url');
  return s;
}
function validerService(id, c, actuel) {
  const r = { ...(actuel || {}) };
  if (c.preset !== undefined) {
    if (!PRESETS[id].some((p) => p.id === c.preset)) throw new Refus(422, 'preset_inconnu', 'Ce préréglage n’existe pas pour ce service.', 'preset');
    r.preset = c.preset;
  }
  if (c.url !== undefined) r.url = validerUrl(c.url);
  for (const k of ['modele', 'voix', 'langue']) {
    if (c[k] === undefined) continue;
    if ((k === 'voix' && id !== 'tts') || (k === 'langue' && id !== 'stt')) continue;
    const t = String(c[k]).trim();
    if (t.length > 80) throw new Refus(422, 'texte_long', 'Ce texte est trop long.', k);
    r[k] = t;
  }
  r.preset = r.preset || 'personnalise';
  if (!r.url) throw new Refus(422, 'url_manquante', 'Indiquez l’adresse du service.', 'url');
  return r;
}
const relire = (id) => {
  const s = etat.services[id];
  return s ? { configure: true, ...s, cle_definie: !!secrets[id] } : { configure: false, cle_definie: false };
};
const affichage = () => ({ ...etat.display, langues: LANGUES, themes: THEMES });

/* ---------- appels sortants : géocodage et météo réels ---------- */
async function geocoder(q) {
  const u = new URL('https://geocoding-api.open-meteo.com/v1/search');
  u.search = new URLSearchParams({ name: q, count: '8', language: 'fr', format: 'json' }).toString();
  let rep;
  try { rep = await fetch(u, { signal: AbortSignal.timeout(8000) }); } catch (e) { throw sansInternet(); }
  if (!rep.ok) throw sansInternet();
  const d = await rep.json().catch(() => ({}));
  return (d.results || []).filter((r) => r.timezone).map((r) => ({
    id: String(r.id), nom: r.name, region: r.admin1 || '', pays: r.country || '', code_pays: r.country_code || '',
    latitude: r.latitude, longitude: r.longitude, altitude: r.elevation ?? null, fuseau: r.timezone, population: r.population ?? null,
    libelle: [r.name, r.admin1, r.country].filter(Boolean).join(', '),
  }));
}
const sansInternet = () => new Refus(503, 'sans_internet', 'L’appareil n’a pas accès à Internet pour chercher un lieu.');
async function meteo() {
  const z = etat.zone;
  if (!z) return { disponible: false };
  const cle = `${z.latitude},${z.longitude}`;
  if (etat.meteo && etat.meteo.cle === cle && Date.now() - etat.meteo.lu < 15 * 60 * 1000) return etat.meteo.rep;
  try {
    const u = new URL('https://api.open-meteo.com/v1/forecast');
    u.search = new URLSearchParams({ latitude: z.latitude, longitude: z.longitude, current: 'temperature_2m,apparent_temperature,is_day,weather_code', timeformat: 'unixtime', timezone: 'auto' }).toString();
    const rep = await fetch(u, { signal: AbortSignal.timeout(8000) });
    if (!rep.ok) throw new Error('statut');
    const c = (await rep.json()).current;
    const r = { disponible: true, perimee: false, mise_a_jour: c.time, actuelle: { temperature: c.temperature_2m, ressentie: c.apparent_temperature, code: c.weather_code, libelle: WMO.get(c.weather_code) || 'Météo inconnue', jour: !!c.is_day } };
    etat.meteo = { cle, lu: Date.now(), rep: r };
    return r;
  } catch (e) {
    if (etat.meteo && etat.meteo.cle === cle) return { ...etat.meteo.rep, perimee: true };
    return { disponible: false };
  }
}

/* ---------- test d'un service : traduit les erreurs en phrases, ne recopie jamais la réponse du service ---------- */
const PHRASE_INJOIGNABLE = 'Le service ne répond pas. Est-il lancé et sur le même réseau ?';
function phraseReseau(e) {
  const code = (e && e.cause && e.cause.code) || (e && e.code) || '';
  if (code === 'ENOTFOUND' || code === 'EAI_AGAIN') return 'Cette adresse est introuvable.';
  if (/CERT|SELF_SIGNED|UNABLE_TO_VERIFY/.test(code)) return 'Le certificat de ce service n’est pas reconnu.';
  return PHRASE_INJOIGNABLE;
}
async function phraseStatut(rep) {
  const s = rep.status;
  const texte = (await rep.text().catch(() => '')).toLowerCase(); // lu pour deviner la cause, jamais transmis
  if (s === 401 || s === 403) return 'Le service refuse la clé. Vérifiez-la.';
  if ((s === 404 || s === 400) && texte.includes('model')) return 'Ce modèle n’existe pas sur ce service.';
  if (s === 404) return 'Cette adresse ne ressemble pas à un service compatible. Il manque peut-être « /v1 ».';
  if (s === 429) return 'Le service limite les appels. Réessayez dans un instant.';
  if (s >= 500) return 'Le service a un problème de son côté.';
  return 'Le service a répondu autre chose que prévu.';
}
function wavSilence() {
  const n = 8000, d = Buffer.alloc(44 + n * 2); // 0,5 s de silence, 16 kHz, 16 bits, mono
  d.write('RIFF', 0); d.writeUInt32LE(36 + n * 2, 4); d.write('WAVEfmt ', 8); d.writeUInt32LE(16, 16); d.writeUInt16LE(1, 20); d.writeUInt16LE(1, 22);
  d.writeUInt32LE(16000, 24); d.writeUInt32LE(32000, 28); d.writeUInt16LE(2, 32); d.writeUInt16LE(16, 34); d.write('data', 36); d.writeUInt32LE(n * 2, 40);
  return d;
}
async function jouerTest(id, p) {
  const t0 = Date.now(), etapes = [];
  const entetes = p.cle ? { Authorization: 'Bearer ' + p.cle } : {};
  const fin = (ok, message, extra = {}) => { tests[id] = { etat: 'termine', ok, duree_ms: Date.now() - t0, message, etapes, ...extra }; };
  let modeles = [];
  try {
    const r = await fetch(p.url + '/models', { headers: entetes, signal: AbortSignal.timeout(10000) });
    if (r.status === 404 || r.status === 405) {
      etapes.push({ nom: 'Joindre le service', ok: true });
    } else if (!r.ok) {
      etapes.push({ nom: 'Joindre le service', ok: false });
      return fin(false, await phraseStatut(r));
    } else {
      const j = await r.json().catch(() => null);
      if (!j || !Array.isArray(j.data)) { etapes.push({ nom: 'Joindre le service', ok: false }); return fin(false, 'Le service a répondu autre chose que prévu.'); }
      modeles = j.data.map((x) => String(x.id)).slice(0, 50);
      etapes.push({ nom: 'Joindre le service', ok: true });
    }
  } catch (e) { etapes.push({ nom: 'Joindre le service', ok: false }); return fin(false, phraseReseau(e)); }
  const modele = p.modele || modeles[0] || 'test';
  try {
    let rep, bonne;
    const json = { ...entetes, 'Content-Type': 'application/json' };
    if (id === 'llm') {
      rep = await fetch(p.url + '/chat/completions', { method: 'POST', headers: json, signal: AbortSignal.timeout(15000), body: JSON.stringify({ model: modele, messages: [{ role: 'user', content: 'Réponds OK.' }], max_tokens: 1 }) });
      bonne = async () => Array.isArray((await rep.json().catch(() => ({}))).choices);
    } else if (id === 'tts') {
      rep = await fetch(p.url + '/audio/speech', { method: 'POST', headers: json, signal: AbortSignal.timeout(15000), body: JSON.stringify({ model: modele, input: 'Bonjour', ...(p.voix ? { voice: p.voix } : {}) }) });
      bonne = async () => (await rep.arrayBuffer()).byteLength > 0;
    } else {
      const f = new FormData();
      f.append('file', new Blob([wavSilence()], { type: 'audio/wav' }), 'silence.wav');
      f.append('model', modele);
      if (p.langue) f.append('language', p.langue);
      rep = await fetch(p.url + '/audio/transcriptions', { method: 'POST', headers: entetes, signal: AbortSignal.timeout(15000), body: f });
      bonne = async () => typeof (await rep.json().catch(() => ({}))).text === 'string';
    }
    if (!rep.ok) { etapes.push({ nom: 'Essai de réponse', ok: false }); return fin(false, await phraseStatut(rep)); }
    if (!(await bonne())) { etapes.push({ nom: 'Essai de réponse', ok: false }); return fin(false, 'Le service a répondu autre chose que prévu.'); }
    etapes.push({ nom: 'Essai de réponse', ok: true });
  } catch (e) { etapes.push({ nom: 'Essai de réponse', ok: false }); return fin(false, phraseReseau(e)); }
  const s = ((Date.now() - t0) / 1000).toFixed(1).replace('.', ',');
  fin(true, `Le service répond en ${s} s.`, { modeles });
}

/* ---------- redémarrage simulé ---------- */
function redemarrer(apres) {
  setTimeout(() => {
    etat.redemarreJusqua = Date.now() + REBOOT_MS;
    sessions.clear(); // les sessions vivent en RAM : un redémarrage les efface (api-reglages.md §2)
    apres();
  }, 500);
}

/* ---------- routeur de l'API ---------- */
async function api(req, res, url) {
  const m = req.method, p = url.pathname;
  if (m === 'GET' && p === '/api/status') {
    return json(res, 200, { nom: etat.nom, modele: 'M5Stack Tab5 (faux appareil)', version: etat.version, mode: 'station', appairage: estConnecte(req) ? 'ok' : 'requis' });
  }
  if (m === 'POST' && p === '/api/pair') {
    if (Date.now() < verrouJusqua) {
      const n = Math.ceil((verrouJusqua - Date.now()) / 1000);
      throw new Refus(429, 'verrouille', `Trop d’essais. Réessayez dans ${n} seconde${n > 1 ? 's' : ''}.`, undefined, { reessayer_dans: n });
    }
    const c = await lireJson(req);
    if (typeof c.code !== 'string' || !egal(c.code, CODE)) {
      if (++echecsCode >= 5) {
        echecsCode = 0; verrouJusqua = Date.now() + VERROU_S * 1000;
        throw new Refus(429, 'verrouille', `Trop d’essais. Réessayez dans ${VERROU_S} secondes.`, undefined, { reessayer_dans: VERROU_S });
      }
      throw new Refus(403, 'code_faux', 'Ce code n’est pas le bon. Regardez l’écran de l’appareil.', 'code');
    }
    echecsCode = 0;
    const jeton = randomBytes(32).toString('hex');
    sessions.set(jeton, Date.now());
    while (sessions.size > 4) sessions.delete(sessions.keys().next().value);
    return json(res, 200, { jeton });
  }
  if (!estConnecte(req)) throw new Refus(401, 'non_appaire', 'Entrez le code affiché sur l’écran de l’appareil.');
  if (m === 'DELETE' && p === '/api/pair') { sessions.delete((req.headers.authorization || '').replace(/^Bearer /, '')); res.writeHead(204); return res.end(); }

  if (m === 'GET' && p === '/api/system') {
    return json(res, 200, { nom: etat.nom, modele: 'M5Stack Tab5 (faux appareil)', version: etat.version, compile_le: '2026-11-06T11:20:00Z', partition: 'ota_0', validee: true,
      depuis_secondes: Math.floor((Date.now() - etat.demarre) / 1000), memoire_libre: 5120000,
      heure: { epoch: Math.floor(Date.now() / 1000), synchronisee: true, fuseau: etat.zone ? etat.zone.fuseau : 'UTC' }, internet: true });
  }
  /* Wi-Fi */
  if (m === 'GET' && p === '/api/wifi') {
    const w = etat.wifi;
    const message = { connecte: `Connecté à « ${w.ssid} ».`, connexion: 'Connexion en cours…', echec: w.message || 'La connexion a échoué.', deconnecte: 'Non connecté.' }[w.etat];
    return json(res, 200, { etat: w.etat, ssid: w.ssid, niveau: w.niveau, force: w.force, ip: w.ip, message });
  }
  if (m === 'POST' && p === '/api/wifi/scan') { etat.scanFin = Date.now() + 800; return json(res, 202, { en_cours: true }); }
  if (m === 'GET' && p === '/api/wifi/scan') {
    const en = Date.now() < etat.scanFin;
    return json(res, 200, en ? { en_cours: true, reseaux: [] } : { en_cours: false, reseaux: [...RESEAUX].sort((a, b) => b.force - a.force) });
  }
  if (m === 'PUT' && p === '/api/wifi') {
    const c = await lireJson(req);
    const ssid = String(c.ssid || '').trim();
    if (!ssid || ssid.length > 32) throw new Refus(422, 'ssid_invalide', 'Le nom du réseau n’est pas valide.', 'ssid');
    const avant = { ...etat.wifi };
    etat.wifi = { ...avant, etat: 'connexion' };
    setTimeout(() => {
      const reseau = RESEAUX.find((r) => r.ssid === ssid);
      const bon = reseau && (!reseau.securise || c.mot_de_passe !== 'mauvais');
      etat.wifi = bon ? { etat: 'connecte', ssid, niveau: reseau.niveau, force: reseau.force, ip: '192.168.0.52' }
        : { ...avant, etat: 'echec', message: reseau ? `Mot de passe refusé par « ${ssid} ». L’ancien réseau est conservé.` : `Le réseau « ${ssid} » est introuvable. L’ancien réseau est conservé.` };
      if (bon) secrets.wifi = c.mot_de_passe || '';
    }, 1200);
    return json(res, 202, { etat: 'connexion' });
  }
  /* zone météo */
  if (m === 'GET' && p === '/api/geocode') {
    const q = (url.searchParams.get('q') || '').trim();
    if (q.length < 3) throw new Refus(400, 'trop_court', 'Tapez au moins 3 lettres.', 'q');
    if (q.length > 80) throw new Refus(400, 'trop_long', 'Cette recherche est trop longue.', 'q');
    return json(res, 200, { resultats: await geocoder(q) });
  }
  if (m === 'GET' && p === '/api/location') return json(res, 200, etat.zone || { definie: false });
  if (m === 'PUT' && p === '/api/location') {
    etat.zone = validerZone(await lireJson(req));
    etat.meteo = null;
    return json(res, 200, etat.zone);
  }
  if (m === 'GET' && p === '/api/weather') return json(res, 200, await meteo());
  /* affichage */
  if (m === 'GET' && p === '/api/display') return json(res, 200, affichage());
  if (m === 'PUT' && p === '/api/display') {
    Object.assign(etat.display, validerAffichage(await lireJson(req)));
    return json(res, 200, affichage());
  }
  /* services */
  if (m === 'GET' && p === '/api/presets') return json(res, 200, PRESETS);
  if (m === 'GET' && p === '/api/services') return json(res, 200, { stt: relire('stt'), llm: relire('llm'), tts: relire('tts') });
  const sv = p.match(/^\/api\/services\/(stt|llm|tts)(\/test)?$/);
  if (sv) {
    const id = sv[1];
    if (!sv[2] && m === 'PUT') {
      const c = await lireJson(req);
      const nouveau = validerService(id, c, etat.services[id]);
      const ancien = etat.services[id];
      if (ancien && c.preset !== undefined && c.preset !== ancien.preset) secrets[id] = ''; // la clé ne suit pas vers un autre fournisseur
      if (typeof c.cle === 'string' && c.cle) secrets[id] = c.cle.slice(0, 400);
      else if (c.effacer_cle === true) secrets[id] = '';
      etat.services[id] = nouveau;
      return json(res, 200, relire(id));
    }
    if (sv[2] && m === 'POST') {
      const c = await lireJson(req, LIMITE_JSON, true);
      const actuel = etat.services[id] || {};
      const url2 = c.url !== undefined ? validerUrl(c.url) : actuel.url;
      if (!url2) throw new Refus(422, 'url_manquante', 'Indiquez d’abord l’adresse du service.', 'url');
      tests[id] = { etat: 'en_cours' };
      const params = { url: url2, modele: String(c.modele ?? actuel.modele ?? ''), voix: String(c.voix ?? actuel.voix ?? ''), langue: String(c.langue ?? actuel.langue ?? ''), cle: typeof c.cle === 'string' && c.cle ? c.cle : secrets[id] };
      jouerTest(id, params).catch(() => { tests[id] = { etat: 'termine', ok: false, duree_ms: 0, message: 'L’appareil a rencontré un problème. Réessayez.', etapes: [] }; });
      return json(res, 202, { etat: 'en_cours' });
    }
    if (sv[2] && m === 'GET') return json(res, 200, tests[id] || { etat: 'inactif' });
  }
  /* export / import sans clés */
  if (m === 'GET' && p === '/api/settings/export') {
    const services = {};
    for (const id of ['stt', 'llm', 'tts']) if (etat.services[id]) services[id] = { ...etat.services[id] };
    const z = etat.zone && { nom: etat.zone.nom, region: etat.zone.region, pays: etat.zone.pays, code_pays: etat.zone.code_pays, latitude: etat.zone.latitude, longitude: etat.zone.longitude, fuseau: etat.zone.fuseau };
    const corps = { format: 'ecran-autonome-reglages', version: 1, exporte_le: new Date().toISOString(), zone: z || null, affichage: { ...etat.display }, services, cles_a_ressaisir: ['stt', 'llm', 'tts'].filter((id) => secrets[id]) };
    return json(res, 200, corps, { 'Content-Disposition': `attachment; filename="reglages-ecran-${new Date().toISOString().slice(0, 10)}.json"` });
  }
  if (m === 'POST' && p === '/api/settings/import') {
    const simuler = url.searchParams.get('simuler') === '1';
    const f = await lireJson(req, LIMITE_IMPORT);
    if (f.format !== 'ecran-autonome-reglages' || !Number.isInteger(f.version) || f.version > 1) throw new Refus(422, 'format', 'Ce fichier ne vient pas de cet écran.');
    const appliques = [], ignores = [], erreurs = [], aFaire = [];
    const essayer = (nom, fn) => { try { const r = fn(); aFaire.push(() => r()); appliques.push(nom); } catch (e) { if (e instanceof Refus) erreurs.push({ champ: nom, message: e.message }); else throw e; } };
    if (f.zone) essayer('zone', () => { const z = validerZone(f.zone); return () => { etat.zone = z; etat.meteo = null; }; });
    if (f.affichage) essayer('affichage', () => { const a = validerAffichage(f.affichage); return () => Object.assign(etat.display, a); });
    for (const id of ['stt', 'llm', 'tts']) {
      const s = f.services && f.services[id];
      if (!s) continue;
      for (const interdit of ['cle', 'mot_de_passe', 'effacer_cle', 'cle_definie']) if (interdit in s) ignores.push({ cle: `services.${id}.${interdit}`, message: 'Les clés ne s’importent pas.' });
      essayer(`services.${id}`, () => { const n = validerService(id, s, undefined); return () => { if (etat.services[id] && etat.services[id].preset !== n.preset) secrets[id] = ''; etat.services[id] = n; }; });
    }
    const cles = (Array.isArray(f.cles_a_ressaisir) ? f.cles_a_ressaisir : []).filter((id) => ['stt', 'llm', 'tts'].includes(id));
    const rep = { simulation: simuler, appliques, ignores, erreurs, cles_a_ressaisir: cles };
    if (simuler) return json(res, 200, rep);
    if (erreurs.length) throw new Refus(422, 'import_refuse', 'Ce fichier contient des réglages refusés : rien n’a été appliqué.');
    aFaire.forEach((fn) => fn());
    return json(res, 200, rep);
  }
  /* mise à jour par la page */
  const maj = () => ({ etat: 'inactif', progression: 0, version: etat.version, validee: true, precedente: { version: etat.precedente, disponible: !!etat.precedente }, dernier_resultat: etat.maj.dernier });
  if (m === 'GET' && p === '/api/update') return json(res, 200, maj());
  if (m === 'POST' && p === '/api/update') {
    const n = Number(req.headers['content-length']);
    if (!Number.isFinite(n) || n <= 0) throw new Refus(400, 'illisible', 'Cette demande n’est pas lisible.');
    if (n > TAILLE_PARTITION) throw new Refus(413, 'trop_gros', 'Ce fichier est trop gros pour l’appareil.');
    const corps = await lire(req, TAILLE_PARTITION);
    if (corps[0] !== 0xe9) throw new Refus(422, 'pas_un_firmware', 'Ce fichier n’est pas un firmware pour cet écran.');
    const cassee = corps.includes('CASSEE'); // image volontairement cassée : le retour arrière automatique la rejette
    json(res, 200, { etat: 'redemarrage', dans_secondes: 3 });
    return redemarrer(() => {
      if (cassee) etat.maj.dernier = { issue: 'annulee', version: etat.version, message: 'La nouvelle version n’a pas démarré correctement : l’ancienne a été remise.' };
      else { etat.precedente = etat.version; etat.version = '0.1.1'; etat.maj.dernier = { issue: 'validee', version: etat.version, message: 'Mise à jour réussie.' }; }
    });
  }
  if (m === 'POST' && p === '/api/update/rollback') {
    if (!etat.precedente) throw new Refus(409, 'pas_de_precedente', 'Il n’y a pas de version précédente à remettre.');
    json(res, 200, { etat: 'redemarrage' });
    return redemarrer(() => {
      [etat.version, etat.precedente] = [etat.precedente, etat.version];
      etat.maj.dernier = { issue: 'validee', version: etat.version, message: `Retour à la version ${etat.version} réussi.` };
    });
  }
  throw new Refus(404, 'inconnue', 'Cette page n’existe pas.');
}

/* ---------- fichiers statiques de la page (gzip comme sur l'appareil) ---------- */
const MIME = { '.html': 'text/html; charset=utf-8', '.css': 'text/css; charset=utf-8', '.js': 'text/javascript; charset=utf-8', '.svg': 'image/svg+xml', '.png': 'image/png', '.ico': 'image/x-icon' };
async function statique(req, res, url) {
  const nom = url.pathname === '/' ? '/index.html' : url.pathname;
  const fichier = path.normalize(path.join(APP, nom));
  if (!fichier.startsWith(APP + path.sep)) throw new Refus(404, 'inconnue', 'Cette page n’existe pas.');
  let corps;
  try { corps = await readFile(fichier); } catch (e) { throw new Refus(404, 'inconnue', 'Cette page n’existe pas.'); }
  const entetes = { 'Content-Type': MIME[path.extname(fichier)] || 'application/octet-stream', 'Cache-Control': 'no-store' };
  if (/\bgzip\b/.test(req.headers['accept-encoding'] || '')) { corps = zlib.gzipSync(corps, { level: 9 }); entetes['Content-Encoding'] = 'gzip'; }
  res.writeHead(200, entetes);
  res.end(corps);
}

const appareil = http.createServer(async (req, res) => {
  try {
    if (Date.now() < etat.redemarreJusqua) return req.socket.destroy(); // l'appareil redémarre : plus personne ne répond
    const url = new URL(req.url, 'http://appareil');
    if (url.pathname.startsWith('/api/')) await api(req, res, url);
    else if (req.method === 'GET') await statique(req, res, url);
    else throw new Refus(404, 'inconnue', 'Cette page n’existe pas.');
  } catch (e) {
    if (res.headersSent) return;
    if (e instanceof Refus) {
      return json(res, e.statut, { erreur: { id: e.id, message: e.message, ...(e.champ ? { champ: e.champ } : {}), ...(e.extra || {}) } });
    }
    console.error(e);
    json(res, 500, { erreur: { id: 'interne', message: 'L’appareil a rencontré un problème. Réessayez.' } });
  }
});

/* ---------- faux service compatible OpenAI (cible du « Tester ») ---------- */
const service = http.createServer(async (req, res) => {
  const cle = (req.headers.authorization || '').replace(/^Bearer /, '');
  await lire(req, 1 << 20).catch(() => null);
  const rep = (s, corps, type = 'application/json') => { res.writeHead(s, { 'Content-Type': type }); res.end(typeof corps === 'string' || Buffer.isBuffer(corps) ? corps : JSON.stringify(corps)); };
  if (cle !== CLE_MOCK) return rep(401, { error: { message: `Incorrect API key provided: ${cle}.` } }); // comme certains vrais services : cite la clé reçue
  const { pathname } = new URL(req.url, 'http://service');
  if (req.method === 'GET' && pathname === '/v1/models') return rep(200, { object: 'list', data: [{ id: 'modele-essai-a' }, { id: 'modele-essai-b' }, { id: 'modele-essai-c' }] });
  if (req.method === 'POST' && pathname === '/v1/chat/completions') return rep(200, { choices: [{ message: { role: 'assistant', content: 'OK' } }] });
  if (req.method === 'POST' && pathname === '/v1/audio/speech') return rep(200, Buffer.alloc(64, 1), 'audio/mpeg');
  if (req.method === 'POST' && pathname === '/v1/audio/transcriptions') return rep(200, { text: '' });
  rep(404, { error: { message: 'unknown route' } });
});

appareil.listen(PORT, '127.0.0.1', () => {
  console.log(`Faux appareil : http://127.0.0.1:${PORT}   (tables du noyau : ${FUSEAUX.size} fuseaux, ${WMO.size} codes météo)`);
  console.log(`Code d'appairage (affiché ici à la place de l'écran) : ${CODE}`);
});
service.listen(PORT_SERVICE, '127.0.0.1', () => console.log(`Faux service OpenAI : http://127.0.0.1:${PORT_SERVICE}/v1   (clé attendue : ${CLE_MOCK})`));
