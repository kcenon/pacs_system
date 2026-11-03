import { defineConfig, devices } from '@playwright/test';

// validation/browser/browser_probe.py starts pacs_server and the built app, then runs this suite with
// their addresses, the tokens and the declared fixtures in the environment.
const output = process.env.PACS_BROWSER_OUTPUT ?? 'test-results';

export default defineConfig({
  testDir: '.',
  outputDir: `${output}/artifacts`,
  // One server holds every test's data; tests own distinct items rather than sharing state.
  workers: 1,
  fullyParallel: false,
  forbidOnly: true,
  retries: 0,
  timeout: 60_000,
  expect: { timeout: 15_000 },
  reporter: [['list'], ['json', { outputFile: `${output}/playwright-report.json` }]],
  use: {
    baseURL: process.env.PACS_WEB_URL,
    trace: 'retain-on-failure',
    screenshot: 'only-on-failure',
    video: 'off',
  },
  projects: [{ name: 'chromium', use: { ...devices['Desktop Chrome'] } }],
});
