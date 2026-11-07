import { execFileSync, spawnSync } from 'node:child_process';
import { readFileSync } from 'node:fs';
import { expect, type APIRequestContext, type Page } from '@playwright/test';

function required(name: string): string {
  const value = process.env[name];
  if (!value) {
    throw new Error(`${name} is not set; run this suite through validation/browser/browser_probe.py`);
  }
  return value;
}

export interface ExpectedSeries {
  series_uid: string;
  number: number;
  modality: string;
  description: string;
  sop_instance_uids: string[];
}

export interface ExpectedStudy {
  study_uid: string;
  study_id: string;
  date: string;
  description: string;
  modality: string;
  series: ExpectedSeries[];
}

export interface ExpectedPatient {
  patient_id: string;
  name: string;
  birth_date: string;
  sex: string;
  protocol: string;
  studies: ExpectedStudy[];
}

export interface Expected {
  patients: ExpectedPatient[];
  dimse: { host: string; port: number; ae: string; calling: string };
}

type TokenName = 'admin' | 'writer' | 'reader' | 'none' | 'expired' | 'wrong_audience' | 'forged';

export const tokens = JSON.parse(readFileSync(required('PACS_TOKENS_FILE'), 'utf8')) as Record<TokenName, string>;
export const expected = JSON.parse(readFileSync(required('PACS_EXPECTED_FILE'), 'utf8')) as Expected;
export const apiUrl = required('PACS_API_URL');
export const databasePath = required('PACS_DB_PATH');

export function patient(id: string): ExpectedPatient {
  const found = expected.patients.find((item) => item.patient_id === id);
  if (!found) {
    throw new Error(`no declared patient ${id}`);
  }
  return found;
}

/** YYYYMMDD as the pages print it, through the app's own formatDate rule. */
export function shownDate(value: string): string {
  return `${value.slice(0, 4)}-${value.slice(4, 6)}-${value.slice(6, 8)}`;
}

export async function signIn(page: Page, token: string) {
  await page.goto('/sign-in');
  await page.getByLabel('Access token').fill(token);
  await page.getByRole('button', { name: 'Sign in' }).click();
}

export async function signedIn(page: Page, token: string) {
  await signIn(page, token);
  await expect(page.getByRole('heading', { name: 'Dashboard' })).toBeVisible();
}

/** The server asked directly, with no page in between: what a page shows has to be there. */
export async function serverJson(request: APIRequestContext, path: string, token: string = tokens.admin) {
  const reply = await request.get(`${apiUrl}${path}`, { headers: { Authorization: `Bearer ${token}` } });
  return { status: reply.status(), body: reply.ok() ? await reply.json() : await reply.text() };
}

/**
 * A Modality Worklist C-FIND through DCMTK findscu, which shares no code with the server or the page.
 * findscu prints the responses only in verbose mode, on stderr, after an echo of the request; the
 * result is one text per pending response, so a key that only the request names is not a match.
 */
export function worklistOverDimse(patientId: string): string[] {
  const { host, port, ae, calling } = expected.dimse;
  const done = spawnSync('findscu', ['-v', '-W', '-aet', calling, '-aec', ae, host, String(port),
    '-k', `PatientID=${patientId}`, '-k', 'PatientName', '-k', 'AccessionNumber',
    '-k', 'ScheduledProcedureStepSequence[0].Modality',
    '-k', 'ScheduledProcedureStepSequence[0].ScheduledStationAETitle',
    '-k', 'ScheduledProcedureStepSequence[0].ScheduledProcedureStepID',
    '-k', 'ScheduledProcedureStepSequence[0].ScheduledProcedureStepStartDate'], { encoding: 'utf8' });
  if (done.status !== 0) {
    throw new Error(`findscu exited ${done.status}: ${done.stderr.slice(-400)}`);
  }
  const output = `${done.stdout}${done.stderr}`;
  if (!/Find Response.*\(Success\)/.test(output)) {
    throw new Error(`findscu did not end with a successful final response: ${output.slice(-400)}`);
  }
  return output.split(/^.*Find Response.*\(Pending\).*$/m).slice(1);
}

/**
 * Runs SQL on the server's own database file from outside the server, to put it in a state it can
 * only fail in. The tests undo every change they make.
 */
export function databaseSql(statement: string) {
  execFileSync('python3', ['-c', 'import sqlite3,sys; c=sqlite3.connect(sys.argv[1], timeout=30); c.execute(sys.argv[2]); c.commit()',
    databasePath, statement]);
}
