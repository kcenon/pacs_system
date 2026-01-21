/**
 * TypeScript type definitions for Viewer State API
 *
 * Viewer states store layout configurations, viewport settings,
 * and window/level settings for study viewing sessions.
 *
 * @see Issue #545 - Implement Annotation & Measurement APIs
 * @see Issue #584 - Part 4: TypeScript Types & Integration Tests
 */

export interface ViewportLayout {
  rows: number;
  columns: number;
}

export interface ViewportState {
  series_uid?: string;
  sop_instance_uid?: string;
  frame_number?: number;
  window_center?: number;
  window_width?: number;
  zoom?: number;
  pan_x?: number;
  pan_y?: number;
  rotation?: number;
  flip_horizontal?: boolean;
  flip_vertical?: boolean;
  invert?: boolean;
}

export interface ViewerStateData {
  layout: ViewportLayout;
  viewports: ViewportState[];
  active_viewport: string;
}

export interface ViewerState {
  state_id: string;
  study_uid: string;
  user_id: string;
  state: ViewerStateData;
  created_at: string;
  updated_at: string;
}

export interface ViewerStateCreateRequest {
  study_uid: string;
  user_id: string;
  layout: ViewportLayout;
  viewports: ViewportState[];
  active_viewport?: string;
}

export interface ViewerStateCreateResponse {
  state_id: string;
  created_at: string;
}

export interface ViewerStateQuery {
  study_uid?: string;
  user_id?: string;
  limit?: number;
}

export interface ViewerStateListResponse {
  data: ViewerState[];
}

export interface RecentStudy {
  user_id: string;
  study_uid: string;
  accessed_at: string;
}

export interface RecentStudiesResponse {
  data: RecentStudy[];
  total: number;
}
