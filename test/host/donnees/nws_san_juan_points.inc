// FICHIER GÉNÉRÉ par test/host/controle_python_fournisseurs.py : ne pas modifier à la main.
// Réponse réelle de NWS api.weather.gov /points (san_juan /points), voir donnees/LISEZMOI.md.
// 3930 octets en 1 morceau(x) de 60000 octets au plus.
R"NWS({
    "@context": [
        "https://geojson.org/geojson-ld/geojson-context.jsonld",
        {
            "@version": "1.1",
            "wx": "https://api.weather.gov/ontology#",
            "s": "https://schema.org/",
            "geo": "http://www.opengis.net/ont/geosparql#",
            "unit": "http://codes.wmo.int/common/unit/",
            "@vocab": "https://api.weather.gov/ontology#",
            "geometry": {
                "@id": "s:GeoCoordinates",
                "@type": "geo:wktLiteral"
            },
            "city": "s:addressLocality",
            "state": "s:addressRegion",
            "distance": {
                "@id": "s:Distance",
                "@type": "s:QuantitativeValue"
            },
            "bearing": {
                "@type": "s:QuantitativeValue"
            },
            "value": {
                "@id": "s:value"
            },
            "unitCode": {
                "@id": "s:unitCode",
                "@type": "@id"
            },
            "forecastOffice": {
                "@type": "@id"
            },
            "forecastGridData": {
                "@type": "@id"
            },
            "publicZone": {
                "@type": "@id"
            },
            "county": {
                "@type": "@id"
            }
        }
    ],
    "id": "https://api.weather.gov/points/18.4655,-66.1057",
    "type": "Feature",
    "geometry": {
        "type": "Point",
        "coordinates": [
            -66.1057,
            18.4655
        ]
    },
    "properties": {
        "@id": "https://api.weather.gov/points/18.4655,-66.1057",
        "@type": "wx:Point",
        "cwa": "SJU",
        "type": "land",
        "forecastOffice": "https://api.weather.gov/offices/SJU",
        "gridId": "SJU",
        "gridX": 162,
        "gridY": 132,
        "forecast": "https://api.weather.gov/gridpoints/SJU/162,132/forecast",
        "forecastHourly": "https://api.weather.gov/gridpoints/SJU/162,132/forecast/hourly",
        "forecastGridData": "https://api.weather.gov/gridpoints/SJU/162,132",
        "observationStations": "https://api.weather.gov/gridpoints/SJU/162,132/stations",
        "relativeLocation": {
            "type": "Feature",
            "geometry": {
                "type": "Point",
                "coordinates": [
                    -66.0637465,
                    18.4045589
                ]
            },
            "properties": {
                "city": "San Juan",
                "state": "PR",
                "distance": {
                    "unitCode": "wmoUnit:m",
                    "value": 0
                },
                "bearing": {
                    "unitCode": "wmoUnit:degree_(angle)",
                    "value": 0
                }
            }
        },
        "forecastZone": "https://api.weather.gov/zones/forecast/PRZ001",
        "county": "https://api.weather.gov/zones/county/PRC127",
        "fireWeatherZone": "https://api.weather.gov/zones/fire/PRZ016",
        "timeZone": "America/Puerto_Rico",
        "radarStation": "TJUA",
        "astronomicalData": {
            "sunrise": "2026-11-07T06:26:55-04:00",
            "sunset": "2026-11-07T17:49:17-04:00",
            "transit": "2026-11-07T12:08:06-04:00",
            "civilTwilightBegin": "2026-11-07T06:04:19-04:00",
            "civilTwilightEnd": "2026-11-07T18:11:53-04:00",
            "nauticalTwilightBegin": "2026-11-07T05:38:11-04:00",
            "nauticalTwilightEnd": "2026-11-07T18:38:01-04:00",
            "astronomicalTwilightBegin": "2026-11-07T05:12:15-04:00",
            "astronomicalTwilightEnd": "2026-11-07T19:03:57-04:00"
        },
        "nwr": {
            "transmitter": "WXJ69",
            "sameCode": "072127",
            "areaBroadcast": "https://api.weather.gov/radio/WXJ69/broadcast",
            "pointBroadcast": "https://api.weather.gov/points/18.4655,-66.1057/radio"
        }
    }
})NWS",
