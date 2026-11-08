import { expect, test } from '@playwright/test';
import { apiUrl, signIn, signedIn, tokens } from './support';

test('signs in with a token and out again, forgetting the token', { tag: '@positive' }, async ({ page }) => {
  await signedIn(page, tokens.reader);
  expect(await page.evaluate(() => sessionStorage.getItem('pacs.accessToken'))).toBe(tokens.reader);
  await page.getByRole('button', { name: 'Sign out' }).click();
  await expect(page).toHaveURL(/\/sign-in$/);
  expect(await page.evaluate(() => sessionStorage.getItem('pacs.accessToken'))).toBeNull();
  await page.goto('/patients');
  await expect(page).toHaveURL(/\/sign-in$/);
});

test('sends no request a page could answer without a token', { tag: '@negative' }, async ({ page, request }) => {
  await page.goto('/patients');
  await expect(page).toHaveURL(/\/sign-in$/);
  const reply = await request.get(`${apiUrl}/api/v1/patients`);
  expect(reply.status()).toBe(401);
  expect(reply.headers()['www-authenticate']).toBe('Bearer');
});

for (const [name, token] of [
  ['an expired token', tokens.expired],
  ['a token for another audience', tokens.wrong_audience],
  ['a token signed with another key', tokens.forged],
] as const) {
  test(`refuses ${name}`, { tag: '@negative' }, async ({ page }) => {
    const answer = page.waitForResponse((reply) => reply.url().endsWith('/api/v1/system/status'));
    await signIn(page, token);
    expect((await answer).status()).toBe(401);
    await expect(page.getByRole('alert')).toContainText('The server refused this token');
    await expect(page).toHaveURL(/\/sign-in$/);
    expect(await page.evaluate(() => sessionStorage.getItem('pacs.accessToken'))).toBeNull();
  });
}

test('refuses a valid token that holds no PACS scope', { tag: '@negative' }, async ({ page }) => {
  const answer = page.waitForResponse((reply) => reply.url().endsWith('/api/v1/system/status'));
  await signIn(page, tokens.none);
  expect((await answer).status()).toBe(403);
  await expect(page.getByRole('alert')).toContainText('holds no PACS scope');
  expect(await page.evaluate(() => sessionStorage.getItem('pacs.accessToken'))).toBeNull();
});

test('a session whose token has expired returns to sign-in', { tag: '@negative' }, async ({ page }) => {
  await page.addInitScript((token) => sessionStorage.setItem('pacs.accessToken', token), tokens.expired);
  const answer = page.waitForResponse((reply) => reply.url().includes('/api/v1/patients'));
  await page.goto('/patients');
  expect((await answer).status()).toBe(401);
  await expect(page).toHaveURL(/\/sign-in$/);
});
