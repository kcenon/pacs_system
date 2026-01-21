/**
 * TypeScript type definitions for Measurement API
 *
 * @see Issue #545 - Implement Annotation & Measurement APIs
 * @see Issue #584 - Part 4: TypeScript Types & Integration Tests
 */

export type MeasurementType =
  | 'length'
  | 'area'
  | 'angle'
  | 'hounsfield'
  | 'suv'
  | 'ellipse_area'
  | 'polygon_area';

export interface Measurement {
  measurement_id: string;
  sop_instance_uid: string;
  frame_number?: number | null;
  user_id: string;
  measurement_type: MeasurementType;
  geometry: object;
  value: number;
  unit: string;
  label?: string;
  created_at: string;
}

export interface MeasurementCreateRequest {
  sop_instance_uid: string;
  frame_number?: number;
  user_id: string;
  measurement_type: MeasurementType;
  geometry: object;
  value: number;
  unit: string;
  label?: string;
}

export interface MeasurementCreateResponse {
  measurement_id: string;
  value: number;
  unit: string;
}

export interface MeasurementQuery {
  sop_instance_uid?: string;
  study_uid?: string;
  user_id?: string;
  measurement_type?: MeasurementType;
  limit?: number;
  offset?: number;
}

export interface MeasurementListResponse {
  data: Measurement[];
  pagination: {
    total: number;
    count: number;
  };
}

export interface MeasurementInstanceResponse {
  data: Measurement[];
}
