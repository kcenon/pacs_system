import { expect, test } from '@playwright/test';
import { databaseSql, expected, patient, serverJson, shownDate, signedIn, tokens } from './support';

test('lists every stored patient with the values that were stored', { tag: '@positive' }, async ({ page, request }) => {
  await signedIn(page, tokens.reader);
  const answer = page.waitForResponse((reply) => reply.url().includes('/api/v1/patients?'));
  await page.getByRole('link', { name: 'Patients' }).click();
  expect((await answer).status()).toBe(200);
  for (const declared of expected.patients) {
    const row = page.getByRole('row', { name: new RegExp(declared.patient_id) });
    await expect(row).toContainText(declared.name);
    await expect(row).toContainText(shownDate(declared.birth_date));
    await expect(row).toContainText(declared.sex);
  }
  const server = await serverJson(request, '/api/v1/patients?limit=100');
  expect(server.body.pagination.total).toBe(expected.patients.length);
  await expect(page.getByText(`Total: ${expected.patients.length} patients`)).toBeVisible();
});

test('filters patients by the start of their name', { tag: '@positive' }, async ({ page }) => {
  await signedIn(page, tokens.reader);
  await page.getByRole('link', { name: 'Patients' }).click();
  const answer = page.waitForResponse((reply) => reply.url().includes('patient_name='));
  await page.getByPlaceholder('Search by patient name...').fill('BROWSER^AL');
  expect((await answer).status()).toBe(200);
  await expect(page.getByRole('row', { name: /BRW-A/ })).toBeVisible();
  await expect(page.getByRole('row', { name: /BRW-B/ })).toHaveCount(0);
  await expect(page.getByRole('row', { name: /BRW-C/ })).toHaveCount(0);
});

test('finds a patient by a Korean name', { tag: '@positive' }, async ({ page }) => {
  const korean = patient('BRW-C');
  await signedIn(page, tokens.reader);
  await page.getByRole('link', { name: 'Patients' }).click();
  await page.getByPlaceholder('Search by patient name...').fill(korean.name);
  await expect(page.getByRole('row', { name: /BRW-C/ })).toContainText(korean.name);
  await expect(page.getByRole('row', { name: /BRW-A/ })).toHaveCount(0);
});

test('says so when no patient matches', { tag: '@negative' }, async ({ page }) => {
  await signedIn(page, tokens.reader);
  await page.getByRole('link', { name: 'Patients' }).click();
  const answer = page.waitForResponse((reply) => reply.url().includes('patient_name='));
  await page.getByPlaceholder('Search by patient name...').fill('NOBODY^HERE');
  const reply = await answer;
  expect(reply.status()).toBe(200);
  expect((await reply.json()).pagination.total).toBe(0);
  await expect(page.getByText('No patients found.')).toBeVisible();
});

test('walks from a patient to studies, series and instances', { tag: '@positive' }, async ({ page, request }) => {
  const alpha = patient('BRW-A');
  await signedIn(page, tokens.reader);
  await page.getByRole('link', { name: 'Patients' }).click();
  await page.getByRole('row', { name: /BRW-A/ }).click();
  for (const study of alpha.studies) {
    await expect(page.getByText(study.description).first()).toBeVisible();
  }
  const study = alpha.studies[0];
  await page.getByText(study.description).first().click();
  for (const series of study.series) {
    await expect(page.getByText(series.description).first()).toBeVisible();
  }
  const series = study.series[0];
  await page.getByText(series.description).first().click();
  const instances = page.getByText(/^Instance \d+$/);
  await expect(instances).toHaveCount(series.sop_instance_uids.length);
  await instances.first().click();
  await expect(page.getByText(series.sop_instance_uids[0]).first()).toBeVisible();
  const server = await serverJson(request, `/api/v1/series/${series.series_uid}/instances`);
  expect(server.body.data.map((row: { sop_instance_uid: string }) => row.sop_instance_uid).sort())
    .toEqual([...series.sop_instance_uids].sort());
});

test('reports a server failure and recovers once it is gone', { tag: '@negative' }, async ({ page }) => {
  await signedIn(page, tokens.reader);
  await page.getByRole('link', { name: 'Patients' }).click();
  await expect(page.getByRole('row', { name: /BRW-A/ })).toBeVisible();
  databaseSql('ALTER TABLE patients RENAME TO patients_hidden_by_test');
  try {
    // The list request of this page, asked after the table is gone: the dashboard asks with limit=1.
    const answer = page.waitForResponse((reply) => reply.url().includes('/api/v1/patients?limit=20'));
    await page.reload();
    const reply = await answer;
    expect(reply.status()).toBeGreaterThanOrEqual(500);
    await expect(page.getByRole('alert')).toContainText(`The server could not load patients (${reply.status()})`);
  } finally {
    databaseSql('ALTER TABLE patients_hidden_by_test RENAME TO patients');
  }
  await page.reload();
  await expect(page.getByRole('row', { name: /BRW-A/ })).toBeVisible();
});
