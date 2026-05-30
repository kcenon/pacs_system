/**
 * TypeScript type definitions for Annotation API
 *
 * @see Issue #545 - Implement Annotation & Measurement APIs
 * @see Issue #584 - Part 4: TypeScript Types & Integration Tests
 */

export type AnnotationType =
  | 'arrow'
  | 'line'
  | 'rectangle'
  | 'ellipse'
  | 'polygon'
  | 'freehand'
  | 'text'
  | 'angle'
  | 'roi';

export interface AnnotationStyle {
  color: string;
  line_width: number;
  fill_color?: string;
  fill_opacity?: number;
  font_family?: string;
  font_size?: number;
}

export interface Annotation {
  annotation_id: string;
  study_uid: string;
  series_uid?: string;
  sop_instance_uid?: string;
  frame_number?: number | null;
  user_id: string;
  annotation_type: AnnotationType;
  geometry: object;
  text?: string;
  style: AnnotationStyle;
  created_at: string;
  updated_at: string;
}

export interface AnnotationCreateRequest {
  study_uid: string;
  series_uid?: string;
  sop_instance_uid?: string;
  frame_number?: number;
  user_id: string;
  annotation_type: AnnotationType;
  geometry: object;
  text?: string;
  style?: Partial<AnnotationStyle>;
}

export interface AnnotationUpdateRequest {
  geometry?: object;
  text?: string;
  style?: Partial<AnnotationStyle>;
}

export interface AnnotationCreateResponse {
  annotation_id: string;
  created_at: string;
}

export interface AnnotationUpdateResponse {
  annotation_id: string;
  updated_at: string;
}

export interface AnnotationQuery {
  study_uid?: string;
  series_uid?: string;
  sop_instance_uid?: string;
  user_id?: string;
  limit?: number;
  offset?: number;
}

export interface AnnotationListResponse {
  data: Annotation[];
  pagination: {
    total: number;
    count: number;
  };
}

export interface AnnotationInstanceResponse {
  data: Annotation[];
}
