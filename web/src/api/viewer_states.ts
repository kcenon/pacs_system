/**
 * Viewer State API client functions
 *
 * @see Issue #545 - Implement Annotation & Measurement APIs
 * @see Issue #584 - Part 4: TypeScript Types & Integration Tests
 */

import axios from 'axios';
import type {
  ViewerState,
  ViewerStateCreateRequest,
  ViewerStateCreateResponse,
  ViewerStateQuery,
  ViewerStateListResponse,
  RecentStudiesResponse,
} from '../types/viewer_state';

const API_BASE_URL = '/api/v1';

export async function createViewerState(
  request: ViewerStateCreateRequest
): Promise<ViewerStateCreateResponse> {
  const response = await axios.post(`${API_BASE_URL}/viewer-states`, request);
  return response.data;
}

export async function getViewerStates(
  query?: ViewerStateQuery
): Promise<ViewerStateListResponse> {
  const response = await axios.get(`${API_BASE_URL}/viewer-states`, {
    params: query,
  });
  return response.data;
}

export async function getViewerState(stateId: string): Promise<ViewerState> {
  const response = await axios.get(
    `${API_BASE_URL}/viewer-states/${encodeURIComponent(stateId)}`
  );
  return response.data;
}

export async function deleteViewerState(stateId: string): Promise<void> {
  await axios.delete(
    `${API_BASE_URL}/viewer-states/${encodeURIComponent(stateId)}`
  );
}

export async function getRecentStudies(
  userId: string,
  limit?: number
): Promise<RecentStudiesResponse> {
  const params: Record<string, number> = {};
  if (limit !== undefined) {
    params.limit = limit;
  }
  const response = await axios.get(
    `${API_BASE_URL}/users/${encodeURIComponent(userId)}/recent-studies`,
    { params }
  );
  return response.data;
}
