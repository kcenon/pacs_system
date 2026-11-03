# Browser flows against the real server

`browser_probe.py` runs the built web app (`web/`) in a real browser against `pacs_server`, with its REST API
on and OAuth 2.0 tokens required (#1322). Nothing stands in for the server: no request is routed or fulfilled
in the browser, which `validation/dcmtk/tests/test_browser_suite.py` checks in the spec files.

## What one run does

1. Makes an RSA key for this run alone, publishes it as a JWKS, and signs tokens with it through openssl
   (`validation/peers/http/http_peer.py`): administrator, writer, reader, no PACS scope, expired, another
   audience, and one signed by another key. The key is deleted when the run ends.
2. Declares the patients, studies, series and objects the pages must show in `expected.json`, before any
   page opens. Patient C has a Korean name in ISO_IR 192.
3. Starts `pacs_server --http-rest-api` and stores patient A over C-STORE (DCMTK `storescu`) and B and C over
   STOW-RS (`curl`), so the data arrives through supported APIs only.
4. Runs `npm run lint` and `npm run build`, then serves the build with `vite preview`, which proxies `/api`
   to the server: the app and the API share one origin, as they would behind one reverse proxy.
5. Runs `npm run test:e2e` (Playwright, `web/tests/e2e`). Each test checks what the page shows against the
   declared values, against the REST API asked directly, and for the worklist against DCMTK `findscu`.
6. Stops the server with SIGTERM and fails if it does not exit 0 by itself.

Each Playwright test becomes one case, `browser.<file>.<positive|negative>.<title>`. `requirement.frontend.positive`
and `requirement.frontend.negative` are passed only when every case of that condition passed.

| Area | Positive | Negative |
|------|----------|----------|
| Sign-in | token kept for the tab, sign-out forgets it | no token, expired, wrong audience, wrong key, no PACS scope, a session that expires |
| Patients | every patient with stored values, prefix search, Korean name, patient to instance | no match, a database that fails and recovers |
| Worklist | create, find over DIMSE MWL, change status, delete | a reader refused, with nothing stored; no items |
| Administration | dashboard totals and health, associations, audit trail with filter and CSV export, settings | audit trail and settings refused to a writer |

The server failure is real: the test renames the `patients` table of the server's own database file from
outside, sees the page report the server's 5xx, renames it back and sees the page recover.

## Running it

In the validation image, or anywhere with a built tree, DCMTK, curl, openssl, pydicom, Node.js 20.19+ and the
browser Playwright names:

```sh
cd web && npm ci && npx playwright install --with-deps --only-shell chromium && cd ..
python3 validation/browser/browser_probe.py --output /tmp/browser --build-dir build \
  --source-revision "$(git rev-parse HEAD)" --source-digest "$(git rev-parse HEAD^{tree})"
```

The driver runs the same thing as `component.browser`. Results go to `--output`: `results.json`,
`browser-summary.json`, `playwright-report.json`, `expected.json`, the server and preview logs, and for a
failed test its screenshot and trace under `artifacts/`. A trace holds the requests of that test, tokens
included; the tokens are useless once the run's key is gone.

## Versions

`@playwright/test` is pinned exactly in `web/package.json` and `web/package-lock.json`; that version names the
browser build it installs (Playwright 1.64.1: Chrome Headless Shell 156.0.8078.4, build 1248). The validation image
installs Node.js and npm from Debian and runs `npm ci` from the lock file.

## Not covered

- No page shows transfer jobs: `pacs_server` has no job manager, and `job_manager` simulates C-STORE jobs (#1323).
- No OpenID Connect redirect: signing in takes a token from the identity provider, so no identity provider
  takes part in a run.
- Chromium only.
