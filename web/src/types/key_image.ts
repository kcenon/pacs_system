/**
 * TypeScript type definitions for Key Image API
 *
 * Key images are significant images marked by users for easy reference,
 * following DICOM PS3.3 Key Object Selection Document patterns.
 *
 * @see Issue #545 - Implement Annotation & Measurement APIs
 * @see Issue #584 - Part 4: TypeScript Types & Integration Tests
 */

export interface KeyImage {
  key_image_id: string;
  study_uid: string;
  sop_instance_uid: string;
  frame_number?: number | null;
  user_id: string;
  reason: string;
  document_title: string;
  created_at: string;
}

export interface KeyImageCreateRequest {
  sop_instance_uid: string;
  frame_number?: number;
  user_id: string;
  reason?: string;
  document_title?: string;
}

export interface KeyImageCreateResponse {
  key_image_id: string;
  created_at: string;
}

export interface KeyImageListResponse {
  data: KeyImage[];
}

export interface ReferencedInstance {
  sop_instance_uid: string;
  frame_number?: number | null;
  reason: string;
}

export interface KeyObjectSelectionDocument {
  document_type: 'Key Object Selection';
  study_uid: string;
  document_title: string;
  referenced_instances: ReferencedInstance[];
  created_at: string;
}
