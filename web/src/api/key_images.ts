/**
 * Key Image API client functions
 *
 * @see Issue #545 - Implement Annotation & Measurement APIs
 * @see Issue #584 - Part 4: TypeScript Types & Integration Tests
 */

import axios from 'axios';
import type {
  KeyImageCreateRequest,
  KeyImageCreateResponse,
  KeyImageListResponse,
  KeyObjectSelectionDocument,
} from '../types/key_image';

const API_BASE_URL = '/api/v1';

export async function createKeyImage(
  studyUid: string,
  request: KeyImageCreateRequest
): Promise<KeyImageCreateResponse> {
  const response = await axios.post(
    `${API_BASE_URL}/studies/${encodeURIComponent(studyUid)}/key-images`,
    request
  );
  return response.data;
}

export async function getStudyKeyImages(
  studyUid: string
): Promise<KeyImageListResponse> {
  const response = await axios.get(
    `${API_BASE_URL}/studies/${encodeURIComponent(studyUid)}/key-images`
  );
  return response.data;
}

export async function deleteKeyImage(keyImageId: string): Promise<void> {
  await axios.delete(
    `${API_BASE_URL}/key-images/${encodeURIComponent(keyImageId)}`
  );
}

export async function exportKeyImagesAsSR(
  studyUid: string
): Promise<KeyObjectSelectionDocument> {
  const response = await axios.post(
    `${API_BASE_URL}/studies/${encodeURIComponent(studyUid)}/key-images/export-sr`
  );
  return response.data;
}
