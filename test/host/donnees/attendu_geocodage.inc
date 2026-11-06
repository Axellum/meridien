// FICHIER GÉNÉRÉ par test/host/capturer_geocodage.py : ne pas modifier à la main.
// Ce que geocodage::analyser doit lire dans chaque capture, calculé par Python (json.loads), pas par le noyau.
// Champs : id_connu, id, nom, region, pays, code_pays, fuseau, latitude, longitude, altitude_connue, altitude,
//          population_connue, population, libelle.
constexpr AttenduLieu ATTENDU_MARSEILLE[] = {
    {true, 2995469LL, R"AT(Marseille)AT", R"AT(Région Provence-Alpes-Côte d'Azur)AT", R"AT(France)AT", R"AT(FR)AT", R"AT(Europe/Paris)AT", 43.29695, 5.38107, true, 28.0, true, 877215LL, R"AT(Marseille, Région Provence-Alpes-Côte d'Azur, France)AT"},
    {true, 2995463LL, R"AT(Marseille-en-Beauvaisis)AT", R"AT(Hauts-de-France)AT", R"AT(France)AT", R"AT(FR)AT", R"AT(Europe/Paris)AT", 49.57579, 1.95491, true, 117.0, true, 990LL, R"AT(Marseille-en-Beauvaisis, Hauts-de-France, France)AT"},
    {true, 4901279LL, R"AT(Marseilles)AT", R"AT(Illinois)AT", R"AT(États-Unis)AT", R"AT(US)AT", R"AT(America/Chicago)AT", 41.33087, -88.70813, true, 159.0, true, 4963LL, R"AT(Marseilles, Illinois, États-Unis)AT"},
    {true, 3721237LL, R"AT(Marseille)AT", R"AT(Nord)AT", R"AT(Haïti)AT", R"AT(HT)AT", R"AT(America/Port-au-Prince)AT", 19.66667, -72.28333, true, 18.0, false, 0LL, R"AT(Marseille, Nord, Haïti)AT"},
    {true, 3721238LL, R"AT(Marseille)AT", R"AT(Nord)AT", R"AT(Haïti)AT", R"AT(HT)AT", R"AT(America/Port-au-Prince)AT", 19.6, -72.35, true, 318.0, false, 0LL, R"AT(Marseille, Nord, Haïti)AT"},
    {true, 3721239LL, R"AT(Marseille)AT", R"AT(Nord)AT", R"AT(Haïti)AT", R"AT(HT)AT", R"AT(America/Port-au-Prince)AT", 19.51667, -72.15, true, 240.0, false, 0LL, R"AT(Marseille, Nord, Haïti)AT"},
    {true, 3721240LL, R"AT(Marseille)AT", R"AT(Nord-Est)AT", R"AT(Haïti)AT", R"AT(HT)AT", R"AT(America/Port-au-Prince)AT", 19.48333, -71.78333, true, 264.0, false, 0LL, R"AT(Marseille, Nord-Est, Haïti)AT"},
    {true, 3721241LL, R"AT(Marseille)AT", R"AT(Nord)AT", R"AT(Haïti)AT", R"AT(HT)AT", R"AT(America/Port-au-Prince)AT", 19.45, -72.15, true, 921.0, false, 0LL, R"AT(Marseille, Nord, Haïti)AT"},
};
constexpr std::size_t N_MARSEILLE = 8;
constexpr AttenduLieu ATTENDU_SAINT_ETIENNE[] = {
    {true, 2980291LL, R"AT(Saint-Étienne)AT", R"AT(Auvergne-Rhône-Alpes)AT", R"AT(France)AT", R"AT(FR)AT", R"AT(Europe/Paris)AT", 45.43389, 4.39, true, 529.0, true, 176280LL, R"AT(Saint-Étienne, Auvergne-Rhône-Alpes, France)AT"},
    {true, 2980230LL, R"AT(Saint-Étienne-en-Dévoluy)AT", R"AT(Région Provence-Alpes-Côte d'Azur)AT", R"AT(France)AT", R"AT(FR)AT", R"AT(Europe/Paris)AT", 44.69362, 5.94063, true, 1274.0, true, 537LL, R"AT(Saint-Étienne-en-Dévoluy, Région Provence-Alpes-Côte d'Azur, France)AT"},
    {true, 2980236LL, R"AT(Saint-Étienne-du-Rouvray)AT", R"AT(Normandie)AT", R"AT(France)AT", R"AT(FR)AT", R"AT(Europe/Paris)AT", 49.37794, 1.10467, true, 14.0, true, 28953LL, R"AT(Saint-Étienne-du-Rouvray, Normandie, France)AT"},
    {true, 2980259LL, R"AT(Saint-Étienne-de-Montluc)AT", R"AT(Pays de la Loire)AT", R"AT(France)AT", R"AT(FR)AT", R"AT(Europe/Paris)AT", 47.27622, -1.78013, true, 24.0, true, 6809LL, R"AT(Saint-Étienne-de-Montluc, Pays de la Loire, France)AT"},
    {true, 2980283LL, R"AT(Saint-Étienne-au-Mont)AT", R"AT(Hauts-de-France)AT", R"AT(France)AT", R"AT(FR)AT", R"AT(Europe/Paris)AT", 50.67794, 1.63084, true, 13.0, true, 5078LL, R"AT(Saint-Étienne-au-Mont, Hauts-de-France, France)AT"},
    {true, 2980219LL, R"AT(Saint-Étienne-lès-Remiremont)AT", R"AT(Grand Est)AT", R"AT(France)AT", R"AT(FR)AT", R"AT(Europe/Paris)AT", 48.02287, 6.60868, true, 397.0, true, 4156LL, R"AT(Saint-Étienne-lès-Remiremont, Grand Est, France)AT"},
    {true, 2980246LL, R"AT(Saint-Étienne-de-Tulmont)AT", R"AT(Occitanie)AT", R"AT(France)AT", R"AT(FR)AT", R"AT(Europe/Paris)AT", 44.05, 1.46667, true, 111.0, true, 2731LL, R"AT(Saint-Étienne-de-Tulmont, Occitanie, France)AT"},
    {true, 2980270LL, R"AT(Saint-Étienne-de-Crossey)AT", R"AT(Auvergne-Rhône-Alpes)AT", R"AT(France)AT", R"AT(FR)AT", R"AT(Europe/Paris)AT", 45.38027, 5.64365, true, 453.0, true, 2699LL, R"AT(Saint-Étienne-de-Crossey, Auvergne-Rhône-Alpes, France)AT"},
};
constexpr std::size_t N_SAINT_ETIENNE = 8;
constexpr AttenduLieu ATTENDU_SPRINGFIELD[] = {
    {true, 4409896LL, R"AT(Springfield)AT", R"AT(Missouri)AT", R"AT(United States)AT", R"AT(US)AT", R"AT(America/Chicago)AT", 37.21533, -93.29824, true, 396.0, true, 170188LL, R"AT(Springfield, Missouri, United States)AT"},
    {true, 4250542LL, R"AT(Springfield)AT", R"AT(Illinois)AT", R"AT(United States)AT", R"AT(US)AT", R"AT(America/Chicago)AT", 39.80172, -89.64371, true, 182.0, true, 114394LL, R"AT(Springfield, Illinois, United States)AT"},
    {true, 4951788LL, R"AT(Springfield)AT", R"AT(Massachusetts)AT", R"AT(United States)AT", R"AT(US)AT", R"AT(America/New_York)AT", 42.10148, -72.58981, true, 25.0, true, 154341LL, R"AT(Springfield, Massachusetts, United States)AT"},
    {true, 4525353LL, R"AT(Springfield)AT", R"AT(Ohio)AT", R"AT(United States)AT", R"AT(US)AT", R"AT(America/New_York)AT", 39.92423, -83.80882, true, 298.0, true, 59680LL, R"AT(Springfield, Ohio, United States)AT"},
    {true, 4659557LL, R"AT(Springfield)AT", R"AT(Tennessee)AT", R"AT(United States)AT", R"AT(US)AT", R"AT(America/Chicago)AT", 36.50921, -86.885, true, 203.0, true, 16808LL, R"AT(Springfield, Tennessee, United States)AT"},
    {true, 4309329LL, R"AT(Springfield)AT", R"AT(Kentucky)AT", R"AT(United States)AT", R"AT(US)AT", R"AT(America/New_York)AT", 37.68534, -85.22218, true, 234.0, true, 3055LL, R"AT(Springfield, Kentucky, United States)AT"},
    {true, 4224162LL, R"AT(Springfield)AT", R"AT(Georgia)AT", R"AT(United States)AT", R"AT(US)AT", R"AT(America/New_York)AT", 32.37241, -81.3115, true, 24.0, true, 2889LL, R"AT(Springfield, Georgia, United States)AT"},
    {true, 5440032LL, R"AT(Springfield)AT", R"AT(Colorado)AT", R"AT(United States)AT", R"AT(US)AT", R"AT(America/Denver)AT", 37.40835, -102.61436, true, 1329.0, true, 1396LL, R"AT(Springfield, Colorado, United States)AT"},
};
constexpr std::size_t N_SPRINGFIELD = 8;
constexpr AttenduLieu ATTENDU_ZURICH[] = {
    {true, 2657896LL, R"AT(Zürich)AT", R"AT(Kanton Zürich)AT", R"AT(Schweiz)AT", R"AT(CH)AT", R"AT(Europe/Zurich)AT", 47.36667, 8.55, true, 429.0, true, 415367LL, R"AT(Zürich, Kanton Zürich, Schweiz)AT"},
    {true, 2743611LL, R"AT(Zurich)AT", R"AT(Friesland)AT", R"AT(Niederlande)AT", R"AT(NL)AT", R"AT(Europe/Amsterdam)AT", 53.11134, 5.39437, true, 2.0, true, 145LL, R"AT(Zurich, Friesland, Niederlande)AT"},
    {true, 5687626LL, R"AT(Zurich)AT", R"AT(Montana)AT", R"AT(Vereinigte Staaten)AT", R"AT(US)AT", R"AT(America/Denver)AT", 48.58444, -109.03044, true, 727.0, false, 0LL, R"AT(Zurich, Montana, Vereinigte Staaten)AT"},
    {true, 4282139LL, R"AT(Zurich)AT", R"AT(Kansas)AT", R"AT(Vereinigte Staaten)AT", R"AT(US)AT", R"AT(America/Chicago)AT", 39.23445, -99.43816, true, 675.0, true, 99LL, R"AT(Zurich, Kansas, Vereinigte Staaten)AT"},
    {true, 2657894LL, R"AT(Zürichberg)AT", R"AT(Kanton Zürich)AT", R"AT(Schweiz)AT", R"AT(CH)AT", R"AT(Europe/Zurich)AT", 47.38572, 8.56687, true, 676.0, false, 0LL, R"AT(Zürichberg, Kanton Zürich, Schweiz)AT"},
    {true, 5145326LL, R"AT(Zurich)AT", R"AT(New York)AT", R"AT(Vereinigte Staaten)AT", R"AT(US)AT", R"AT(America/New_York)AT", 43.15062, -77.0433, true, 142.0, false, 0LL, R"AT(Zurich, New York, Vereinigte Staaten)AT"},
    {true, 5411240LL, R"AT(Zurich)AT", R"AT(Kalifornien)AT", R"AT(Vereinigte Staaten)AT", R"AT(US)AT", R"AT(America/Los_Angeles)AT", 37.18271, -118.2601, true, 1199.0, false, 0LL, R"AT(Zurich, Kalifornien, Vereinigte Staaten)AT"},
    {true, 6186201LL, R"AT(Zurich)AT", R"AT(Ontario)AT", R"AT(Kanada)AT", R"AT(CA)AT", R"AT(America/Toronto)AT", 43.42163, -81.62773, true, 260.0, false, 0LL, R"AT(Zurich, Ontario, Kanada)AT"},
};
constexpr std::size_t N_ZURICH = 8;
constexpr AttenduLieu ATTENDU_ISTANBUL[] = {
    {true, 745044LL, R"AT(İstanbul)AT", R"AT(İstanbul)AT", R"AT(Türkiye Cumhuriyeti)AT", R"AT(TR)AT", R"AT(Europe/Istanbul)AT", 41.01384, 28.94966, true, 39.0, true, 15701602LL, R"AT(İstanbul, İstanbul, Türkiye Cumhuriyeti)AT"},
    {true, 10953465LL, R"AT(İstanbulboğazı)AT", R"AT(Ordu)AT", R"AT(Türkiye Cumhuriyeti)AT", R"AT(TR)AT", R"AT(Europe/Istanbul)AT", 41.0641, 37.71004, true, 433.0, false, 0LL, R"AT(İstanbulboğazı, Ordu, Türkiye Cumhuriyeti)AT"},
    {true, 264407LL, R"AT(İstanbulya Ada)AT", R"AT(South Aegean)AT", R"AT(Yunanistan)AT", R"AT(GR)AT", R"AT(Europe/Athens)AT", 36.5758, 26.36221, true, 8.0, false, 0LL, R"AT(İstanbulya Ada, South Aegean, Yunanistan)AT"},
    {true, 11838481LL, R"AT(İstanbul Havalimanı)AT", R"AT(İstanbul)AT", R"AT(Türkiye Cumhuriyeti)AT", R"AT(TR)AT", R"AT(Europe/Istanbul)AT", 41.2629, 28.74242, true, 125.0, false, 0LL, R"AT(İstanbul Havalimanı, İstanbul, Türkiye Cumhuriyeti)AT"},
    {true, 12908718LL, R"AT(Istanbul Old Town)AT", R"AT(İstanbul)AT", R"AT(Türkiye Cumhuriyeti)AT", R"AT(TR)AT", R"AT(Europe/Istanbul)AT", 41.00783, 28.97838, false, 0.0, false, 0LL, R"AT(Istanbul Old Town, İstanbul, Türkiye Cumhuriyeti)AT"},
    {true, 6299743LL, R"AT(İstanbul Atatürk Havalimanı)AT", R"AT(İstanbul)AT", R"AT(Türkiye Cumhuriyeti)AT", R"AT(TR)AT", R"AT(Europe/Istanbul)AT", 40.97457, 28.82197, true, 49.0, false, 0LL, R"AT(İstanbul Atatürk Havalimanı, İstanbul, Türkiye Cumhuriyeti)AT"},
};
constexpr std::size_t N_ISTANBUL = 6;
constexpr AttenduLieu ATTENDU_PARIS[] = {
    {true, 2988507LL, R"AT(Paris)AT", R"AT(Île-de-France)AT", R"AT(France)AT", R"AT(FR)AT", R"AT(Europe/Paris)AT", 48.85341, 2.3488, true, 42.0, true, 2138551LL, R"AT(Paris, Île-de-France, France)AT"},
    {true, 4717560LL, R"AT(Paris)AT", R"AT(Texas)AT", R"AT(États-Unis)AT", R"AT(US)AT", R"AT(America/Chicago)AT", 33.66094, -95.55551, true, 183.0, true, 24782LL, R"AT(Paris, Texas, États-Unis)AT"},
    {true, 4647963LL, R"AT(Paris)AT", R"AT(Tennessee)AT", R"AT(États-Unis)AT", R"AT(US)AT", R"AT(America/Chicago)AT", 36.302, -88.32671, true, 157.0, true, 10150LL, R"AT(Paris, Tennessee, États-Unis)AT"},
    {true, 4303602LL, R"AT(Paris)AT", R"AT(Kentucky)AT", R"AT(États-Unis)AT", R"AT(US)AT", R"AT(America/New_York)AT", 38.2098, -84.25299, true, 257.0, true, 9870LL, R"AT(Paris, Kentucky, États-Unis)AT"},
    {true, 4246659LL, R"AT(Paris)AT", R"AT(Illinois)AT", R"AT(États-Unis)AT", R"AT(US)AT", R"AT(America/Chicago)AT", 39.61115, -87.69614, true, 220.0, true, 8432LL, R"AT(Paris, Illinois, États-Unis)AT"},
    {true, 4974617LL, R"AT(Paris)AT", R"AT(Maine)AT", R"AT(États-Unis)AT", R"AT(US)AT", R"AT(America/New_York)AT", 44.25979, -70.50062, true, 247.0, true, 5073LL, R"AT(Paris, Maine, États-Unis)AT"},
    {true, 4402452LL, R"AT(Paris)AT", R"AT(Missouri)AT", R"AT(États-Unis)AT", R"AT(US)AT", R"AT(America/Chicago)AT", 39.48087, -92.00128, true, 211.0, true, 1176LL, R"AT(Paris, Missouri, États-Unis)AT"},
    {true, 5603240LL, R"AT(Paris)AT", R"AT(Idaho)AT", R"AT(États-Unis)AT", R"AT(US)AT", R"AT(America/Boise)AT", 42.22715, -111.40104, true, 1818.0, true, 506LL, R"AT(Paris, Idaho, États-Unis)AT"},
};
constexpr std::size_t N_PARIS = 8;
constexpr AttenduLieu ATTENDU_AUCUN_RESULTAT[] = {
    {false, 0LL, "", "", "", "", "", 0.0, 0.0, false, 0.0, false, 0LL, ""}, // aucun lieu
};
constexpr std::size_t N_AUCUN_RESULTAT = 0;
