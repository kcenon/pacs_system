import { expect, test } from '@playwright/test';
import { serverJson, signedIn, tokens, worklistOverDimse } from './support';

interface WorklistRow {
  pk: number;
  step_id: string;
  patient_id: string;
  step_status: string;
}

async function itemsFor(request: Parameters<typeof serverJson>[0], patientId: string): Promise<WorklistRow[]> {
  const server = await serverJson(request, `/api/v1/worklist?patient_id=${patientId}&include_all_status=true`);
  expect(server.status).toBe(200);
  return server.body.data;
}

test('schedules a procedure that a DICOM modality then finds, changes it and removes it', { tag: '@positive' }, async ({ page, request }) => {
  const patientId = `WL-${Date.now()}`;
  await signedIn(page, tokens.writer);
  await page.getByRole('link', { name: 'Worklist' }).click();
  await page.getByRole('button', { name: 'New Item' }).click();
  await page.getByLabel('Step ID').fill(`STEP-${patientId}`);
  await page.getByLabel('Patient ID').fill(patientId);
  await page.getByLabel('Patient Name').fill('WORKLIST^ONE');
  await page.getByLabel('Modality').selectOption('CT');
  await page.getByLabel('Station AE Title').fill('BROWSER_CT');
  await page.getByLabel('Scheduled Date/Time').fill('2026-12-01T09:30');
  const created = page.waitForResponse((reply) => reply.url().endsWith('/api/v1/worklist') && reply.request().method() === 'POST');
  await page.getByRole('button', { name: 'Create', exact: true }).click();
  expect((await created).status()).toBe(201);
  await expect(page.getByRole('row', { name: new RegExp(patientId) })).toBeVisible();

  const [row] = await itemsFor(request, patientId);
  expect(row.step_id).toBe(`STEP-${patientId}`);
  const dimse = worklistOverDimse(patientId);
  expect(dimse).toHaveLength(1);
  expect(dimse[0]).toContain(patientId);
  expect(dimse[0]).toContain('BROWSER_CT');
  expect(dimse[0]).toContain(`STEP-${patientId}`);

  await page.getByRole('row', { name: new RegExp(patientId) }).getByRole('button', { name: 'Update Status' }).click();
  const updated = page.waitForResponse((reply) => reply.url().endsWith(`/api/v1/worklist/${row.pk}`) && reply.request().method() === 'PUT');
  await page.getByLabel('New Status').selectOption('STARTED');
  await page.getByRole('button', { name: 'Update Status' }).last().click();
  expect((await updated).status()).toBe(200);
  expect((await itemsFor(request, patientId))[0].step_status).toBe('STARTED');

  await page.getByRole('row', { name: new RegExp(patientId) }).getByRole('button', { name: 'Delete' }).click();
  const deleted = page.waitForResponse((reply) => reply.url().endsWith(`/api/v1/worklist/${row.pk}`) && reply.request().method() === 'DELETE');
  await page.getByRole('button', { name: 'Delete' }).last().click();
  expect((await deleted).status()).toBeLessThan(300);
  await expect(page.getByRole('row', { name: new RegExp(patientId) })).toHaveCount(0);
  expect(await itemsFor(request, patientId)).toHaveLength(0);
  expect(worklistOverDimse(patientId)).toHaveLength(0);
});

test('a token that may only read cannot schedule anything', { tag: '@negative' }, async ({ page, request }) => {
  const patientId = `WL-READER-${Date.now()}`;
  await signedIn(page, tokens.reader);
  await page.getByRole('link', { name: 'Worklist' }).click();
  await page.getByRole('button', { name: 'New Item' }).click();
  await page.getByLabel('Step ID').fill(`STEP-${patientId}`);
  await page.getByLabel('Patient ID').fill(patientId);
  await page.getByLabel('Modality').selectOption('CT');
  await page.getByLabel('Scheduled Date/Time').fill('2026-12-01T09:30');
  const refused = page.waitForResponse((reply) => reply.url().endsWith('/api/v1/worklist') && reply.request().method() === 'POST');
  await page.getByRole('button', { name: 'Create', exact: true }).click();
  expect((await refused).status()).toBe(403);
  await expect(page.getByRole('alert').filter({ hasText: 'Could not create the item' })).toContainText('Insufficient scope');
  expect(await itemsFor(request, patientId)).toHaveLength(0);
  expect(worklistOverDimse(patientId)).toHaveLength(0);
});

test('says so when the worklist holds nothing for a patient', { tag: '@negative' }, async ({ page }) => {
  await signedIn(page, tokens.reader);
  await page.getByRole('link', { name: 'Worklist' }).click();
  const answer = page.waitForResponse((reply) => reply.url().includes('patient_id=NOBODY-'));
  await page.getByPlaceholder('Search by patient ID...').fill(`NOBODY-${Date.now()}`);
  expect((await answer).status()).toBe(200);
  await expect(page.getByText('No scheduled procedures found.')).toBeVisible();
});
