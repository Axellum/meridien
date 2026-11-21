### Added

- **« Is it normal? » on the home screen**: a pill on the place line says how today's forecast maximum compares to the Météo-France
  normal of the month (« +1,0° sur la normale », « −4,2° sous la normale », « Dans la normale », or a broken heat or cold record).
  The device asks Vigie once a day for the normals of the month (`GET /v1/climate/normals`, no temperature in the request) and compares
  on the screen itself; no pill outside France and its overseas territories, when the answer is not for the local month or when the
  alerts are off. Seven languages. ([ADR-0029](../docs/decisions/0029-accueil-est-ce-normal-et-page-sources.md)) Tested on: PC renders and
  unit tests only; not tested on board.
- **« Sources » page in the press-and-hold wheel**: lists what the screen really shows (weather, credited to the service that gave it;
  places; Vigie alerts; the normals with their sheet date and Licence Ouverte 2.0; sea and spots; fonts and icons) with the licence
  sentence of each. The sentences are now one table shared with the settings page. Tested on: PC renders only; not tested on board.
