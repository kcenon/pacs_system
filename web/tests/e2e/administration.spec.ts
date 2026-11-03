import { readFileSync } from 'node:fs';
import { expect, test } from '@playwright/test';
import { expected, serverJson, signedIn, tokens } from './support';

test('the dashboard shows the counts and health the server reports', { tag: '@positive' }, async ({ page, request }) => {
  const [patients, studies, status] = await Promise.all([
    serverJson(request, '/api/v1/patients?limit=1'),
    serverJson(request, '/api/v1/studies?limit=1'),
    serverJson(request, '/api/v1/system/status'),
  ]);
  expect(patients.body.pagination.total).toBe(expected.patients.length);
  await signedIn(page, tokens.reader);
  await expect(page.getByText('Total Patients').locator('..').locator('..')).toContainText(String(patients.body.pagination.total));
  await expect(page.getByText('Total Studies').locator('..').locator('..')).toContainText(String(studies.body.pagination.total));
  // Whatever the server reports, the page shows: here the disk can be full enough to read degraded.
  const level: string = status.body.status;
  await expect(page.getByText(level.charAt(0).toUpperCase() + level.slice(1)).first()).toBeVisible();
  expect(status.body.database.connected).toBe(true);
  await expect(page.getByText('database', { exact: false }).first()).toBeVisible();
});

test('associations show what the DICOM server counted', { tag: '@positive' }, async ({ page, request }) => {
  const server = await serverJson(request, '/api/v1/associations/active');
  expect(server.status).toBe(200);
  await signedIn(page, tokens.reader);
  await page.getByRole('link', { name: 'Associations' }).click();
  await expect(page.getByTestId('association-total-associations')).toHaveText(server.body.total_associations.toLocaleString('en-US'));
  await expect(page.getByText('Running')).toBeVisible();
});

test('the audit trail lists the stores and can be filtered and exported', { tag: '@positive' }, async ({ page }) => {
  await signedIn(page, tokens.admin);
  await page.getByRole('link', { name: 'Audit' }).click();
  const filtered = page.waitForResponse((reply) => reply.url().includes('event_type=C_STORE'));
  await page.getByRole('combobox').first().selectOption('C_STORE');
  const reply = await filtered;
  expect(reply.status()).toBe(200);
  const rows = (await reply.json()).data as { event_type: string; patient_id: string; outcome: string }[];
  const stored = expected.patients.filter((item) => item.protocol === 'C-STORE').map((item) => item.patient_id);
  const declared = expected.patients.filter((item) => item.protocol === 'C-STORE')
    .flatMap((item) => item.studies.flatMap((study) => study.series.flatMap((series) => series.sop_instance_uids)));
  expect(rows).toHaveLength(declared.length);
  expect(rows.every((row) => row.event_type === 'C_STORE')).toBe(true);
  for (const id of stored) {
    expect(rows.some((row) => row.patient_id === id && row.outcome === 'SUCCESS')).toBe(true);
  }
  // The table names each stored object in its message, and the calling AE that sent it.
  for (const sop of declared) {
    await expect(page.getByRole('row', { name: new RegExp(sop.replace(/\./g, '\\.')) })).toContainText(expected.dimse.calling);
  }
  const download = page.waitForEvent('download');
  await page.getByRole('button', { name: /CSV/ }).click();
  const csv = readFileSync(await (await download).path(), 'utf8');
  expect(csv.split('\n')[0]).toContain('event_type');
  for (const id of stored) {
    expect(csv).toContain(id);
  }
});

test('the audit trail and settings are for administrators only', { tag: '@negative' }, async ({ page }) => {
  await signedIn(page, tokens.writer);
  const audit = page.waitForResponse((reply) => reply.url().includes('/api/v1/audit/logs'));
  await page.getByRole('link', { name: 'Audit' }).click();
  expect((await audit).status()).toBe(403);
  await expect(page.getByRole('alert')).toContainText('does not allow viewing audit logs');
  const config = page.waitForResponse((reply) => reply.url().endsWith('/api/v1/system/config'));
  await page.getByRole('link', { name: /Config/ }).click();
  expect((await config).status()).toBe(403);
  await expect(page.getByRole('alert')).toContainText('does not allow viewing the settings');
});

test('settings show what the running server uses', { tag: '@positive' }, async ({ page, request }) => {
  const server = await serverJson(request, '/api/v1/system/config');
  await signedIn(page, tokens.admin);
  await page.getByRole('link', { name: /Config/ }).click();
  await expect(page.getByText('Port').locator('..')).toContainText(String(server.body.port));
  await expect(page.getByText('Bind address').locator('..')).toContainText(server.body.bind_address);
  await expect(page.getByRole('button', { name: /Save/ })).toHaveCount(0);
});
