"""Build deterministic DICOM fixtures for the corpus (#1318); needs pydicom and numpy.

Recipes follow PS3.3 IOD tables and are checked by dicom3tools dciodvfy, which
knows nothing about this project. Generated files are preserved with the run's
evidence, so every result can be reproduced from its source object.
"""
from pydicom.dataset import Dataset, FileMetaDataset
from pydicom.sequence import Sequence
from pydicom.uid import ExplicitVRLittleEndian
import numpy as np

from corpus import PIXEL_FORMATS, STORAGE_SOPS

DATE, TIME, DATETIME = '20261101', '120000', '20261101120000'
ORIENTATION = ['1', '0', '0', '0', '1', '0']
PDF = (b'%PDF-1.4\n1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj\n2 0 obj<</Type/Pages/Kids[]/Count 0>>endobj\n'
       b'trailer<</Root 1 0 R>>\n%%EOF\n')
CDA = (b'<?xml version="1.0"?><ClinicalDocument xmlns="urn:hl7-org:v3"><title>PACS corpus</title>'
       b'</ClinicalDocument>')


def item(**attributes):
    ds = Dataset()
    for key, value in attributes.items():
        setattr(ds, key, value)
    return ds


def code(value, scheme, meaning):
    result = item(CodeValue=value, CodingSchemeDesignator=scheme, CodeMeaning=meaning)
    if scheme == 'SCPECG':
        result.CodingSchemeVersion = '1.3'
    return result


def sequence(*items):
    return Sequence(list(items))


def pixel_array(format_, frames):
    """A gradient with bounded noise and both range extremes, distinct per frame."""
    rng = np.random.default_rng(1318 + frames + sum(map(ord, format_.key)))
    rows, columns, samples = format_.rows, format_.columns, format_.samples
    if format_.key == 'float32':
        return (np.linspace(-1.5, 1.5, frames * rows * columns, dtype='<f4')
                .reshape(frames, rows, columns))
    if format_.bits_allocated == 1:
        pattern = np.indices((frames, rows, columns)).sum(axis=0) % 2
        return pattern.astype('u1')
    if format_.signed:
        low, high = -(1 << (format_.bits_stored - 1)), (1 << (format_.bits_stored - 1)) - 1
    else:
        low, high = 0, (1 << format_.bits_stored) - 1
    shape = (frames, rows, columns, samples) if samples > 1 else (frames, rows, columns)
    span = np.linspace(low, high, int(np.prod(shape)), dtype='f8').reshape(shape)
    # Noise within 1/64 of the range keeps codestreams small but non-trivial.
    limit = max(2, (high - low) // 64)
    values = np.clip(span + rng.integers(-limit, limit + 1, shape), low, high).astype('i8')
    for frame in range(frames):
        values[frame] = np.roll(values[frame], frame * 3, axis=0)
        values[frame].flat[0] = low
        values[frame].flat[-1] = high
    kind = {8: 'i1' if format_.signed else 'u1', 16: '<i2' if format_.signed else '<u2',
            32: '<u4'}[format_.bits_allocated]
    return values.astype(kind)


def native_bytes(format_, frames):
    array = pixel_array(format_, frames)
    if format_.bits_allocated == 1:
        return np.packbits(array.reshape(-1), bitorder='little').tobytes()
    if format_.samples > 1 and format_.planar == 1:
        array = np.moveaxis(array, -1, 1)
    data = array.tobytes()
    return data + b'\0' if len(data) % 2 else data


def attach_pixels(ds, format_key, frames, frame_attribute=True):
    format_ = PIXEL_FORMATS[format_key]
    ds.Rows, ds.Columns = format_.rows, format_.columns
    ds.SamplesPerPixel = format_.samples
    ds.PhotometricInterpretation = format_.photometric
    ds.BitsAllocated, ds.BitsStored = format_.bits_allocated, format_.bits_stored
    ds.HighBit = format_.bits_stored - 1
    ds.PixelRepresentation = 1 if format_.signed and format_.key != 'float32' else 0
    if format_.samples > 1:
        ds.PlanarConfiguration = format_.planar
    if frame_attribute and frames > 1:
        ds.NumberOfFrames = frames
    if format_.key == 'float32':
        del ds.BitsStored, ds.HighBit, ds.PixelRepresentation
        ds.FloatPixelData = native_bytes(format_, frames)
    else:
        ds.PixelData = native_bytes(format_, frames)
        ds['PixelData'].VR = 'OW' if format_.bits_allocated > 8 else 'OB'


def common(ds, sop, uids, modality):
    ds.SpecificCharacterSet = 'ISO_IR 100'
    ds.SOPClassUID, ds.SOPInstanceUID = sop.uid, uids['instance']
    ds.InstanceCreationDate, ds.InstanceCreationTime = DATE, TIME
    ds.PatientName, ds.PatientID = 'CORPUS^FIXTURE', 'PACS-CORPUS'
    ds.PatientBirthDate, ds.PatientSex = '', ''
    ds.StudyInstanceUID, ds.SeriesInstanceUID = uids['study'], uids['series']
    ds.StudyDate, ds.StudyTime, ds.StudyID = DATE, TIME, '1'
    ds.AccessionNumber, ds.ReferringPhysicianName = '', ''
    ds.Modality, ds.SeriesNumber, ds.InstanceNumber = modality, 1, 1
    ds.Manufacturer = 'PACS_CORPUS'
    ds.ContentDate, ds.ContentTime = DATE, TIME


def legacy_laterality(ds):
    ds.Laterality = ''


def enhanced_equipment(ds):
    ds.ManufacturerModelName, ds.DeviceSerialNumber, ds.SoftwareVersions = 'CORPUS', '1', '1'


def frame_of_reference(ds, uids):
    ds.FrameOfReferenceUID = uids['frame']
    ds.PositionReferenceIndicator = ''


def image_plane(ds):
    ds.PixelSpacing = ['1', '1']
    ds.ImageOrientationPatient = ORIENTATION
    ds.ImagePositionPatient = ['0', '0', '0']
    ds.SliceThickness = '1'


def general_image(ds, image_type=('ORIGINAL', 'PRIMARY')):
    ds.ImageType = list(image_type)
    ds.PatientOrientation = ['L', 'F']
    ds.AcquisitionDate, ds.AcquisitionTime = DATE, TIME
    ds.AcquisitionNumber = 1


def rescale(ds, intercept='0', slope='1', kind='US'):
    ds.RescaleIntercept, ds.RescaleSlope, ds.RescaleType = intercept, slope, kind


def voi(ds, format_key):
    format_ = PIXEL_FORMATS[format_key]
    width = 1 << format_.bits_stored
    ds.WindowCenter, ds.WindowWidth = str(0 if format_.signed else width // 2), str(width)


def lossless_flag(ds):
    ds.LossyImageCompression = '00'


# Legacy single-frame and cine IODs ---------------------------------------------------

def build_ct(ds, sop, uids):
    common(ds, sop, uids, 'CT')
    legacy_laterality(ds)
    frame_of_reference(ds, uids)
    general_image(ds, ('ORIGINAL', 'PRIMARY', 'AXIAL'))
    ds.PatientPosition = 'HFS'
    del ds.PatientOrientation
    image_plane(ds)
    ds.KVP = '120'
    rescale(ds, '-1024', '1', 'HU')
    voi(ds, sop.pixel_format)
    attach_pixels(ds, sop.pixel_format, sop.frames)


def build_mr(ds, sop, uids):
    common(ds, sop, uids, 'MR')
    legacy_laterality(ds)
    frame_of_reference(ds, uids)
    general_image(ds, ('ORIGINAL', 'PRIMARY', 'OTHER'))
    ds.PatientPosition = 'HFS'
    del ds.PatientOrientation
    image_plane(ds)
    ds.ScanningSequence, ds.SequenceVariant, ds.ScanOptions = 'SE', 'NONE', ''
    ds.MRAcquisitionType = '2D'
    ds.RepetitionTime, ds.EchoTime, ds.EchoTrainLength = '500', '10', 1
    attach_pixels(ds, sop.pixel_format, sop.frames)


def build_us(ds, sop, uids):
    common(ds, sop, uids, 'US')
    legacy_laterality(ds)
    general_image(ds, ('ORIGINAL', 'PRIMARY', 'ABDOMINAL', '0001'))
    lossless_flag(ds)
    attach_pixels(ds, sop.pixel_format, sop.frames)


def build_us_multiframe(ds, sop, uids):
    build_us(ds, sop, uids)
    ds.FrameTime = '33.3'
    ds.FrameIncrementPointer = (0x0018, 0x1063)


def build_xa(ds, sop, uids):
    common(ds, sop, uids, 'XA')
    legacy_laterality(ds)
    general_image(ds, ('ORIGINAL', 'PRIMARY', 'SINGLE PLANE'))
    ds.PositionerMotion = 'STATIC'
    ds.KVP, ds.ExposureTime, ds.XRayTubeCurrent = '80', '10', '100'
    ds.DistanceSourceToDetector, ds.DistanceSourceToPatient = '1000', '700'
    ds.IntensifierSize = '230'
    ds.RadiationSetting, ds.PixelIntensityRelationship = 'GR', 'LIN'
    ds.PositionerPrimaryAngle, ds.PositionerSecondaryAngle = '0', '0'
    ds.FrameTime = '33.3'
    ds.FrameIncrementPointer = (0x0018, 0x1063)
    ds.FrameDelay, ds.CineRate = '0', 30
    lossless_flag(ds)
    attach_pixels(ds, sop.pixel_format, sop.frames)


def build_xrf(ds, sop, uids):
    common(ds, sop, uids, 'RF')
    legacy_laterality(ds)
    general_image(ds, ('ORIGINAL', 'PRIMARY', 'SINGLE PLANE'))
    ds.PositionerMotion = 'STATIC'
    ds.KVP, ds.ExposureTime, ds.XRayTubeCurrent = '80', '10', '100'
    ds.RadiationSetting, ds.PixelIntensityRelationship = 'GR', 'LIN'
    ds.DistanceSourceToDetector, ds.DistanceSourceToPatient = '1000', '700'
    ds.ColumnAngulation, ds.TableMotion, ds.TableAngle = '0', 'STATIC', '0'
    ds.FrameTime = '33.3'
    ds.FrameIncrementPointer = (0x0018, 0x1063)
    lossless_flag(ds)
    attach_pixels(ds, sop.pixel_format, sop.frames)


def _dx_common(ds, sop, uids, modality):
    processing = sop.name.endswith('For Processing')
    common(ds, sop, uids, modality)
    ds.ImageLaterality = 'U'
    ds.AcquisitionContextSequence = sequence()
    ds.PositionerType = ''
    ds.PresentationIntentType = 'FOR PROCESSING' if processing else 'FOR PRESENTATION'
    ds.ImageType = ['ORIGINAL', 'PRIMARY']
    ds.PatientOrientation = ['L', 'F']
    ds.AcquisitionDate, ds.AcquisitionTime = DATE, TIME
    ds.AcquisitionDateTime = DATETIME
    ds.AcquisitionNumber = 1
    ds.ContentDate, ds.ContentTime = DATE, TIME
    ds.BurnedInAnnotation, ds.LossyImageCompression = 'NO', '00'
    ds.PixelIntensityRelationship = 'LIN' if processing else 'LOG'
    ds.PixelIntensityRelationshipSign = 1
    ds.PresentationLUTShape = 'IDENTITY'
    ds.RescaleIntercept, ds.RescaleSlope, ds.RescaleType = '0', '1', 'US'
    ds.ImagerPixelSpacing = ['0.1', '0.1']
    ds.DetectorType, ds.DetectorConfiguration = 'SCINTILLATOR', 'AREA'
    ds.DetectorBinning = ['1', '1']
    ds.DetectorActiveShape = 'RECTANGLE'
    ds.DetectorActiveDimensions = ['10', '10']
    ds.ContentQualification = 'RESEARCH'
    ds.FieldOfViewShape = 'RECTANGLE'
    ds.FieldOfViewDimensions = ['10', '10']
    ds.KVP, ds.ExposureTime, ds.XRayTubeCurrent = '80', '10', '100'
    ds.Exposure = '1'
    if not processing:
        ds.WindowCenter, ds.WindowWidth = '128', '256'
    return processing


def build_dx(ds, sop, uids):
    _dx_common(ds, sop, uids, 'DX')
    ds.BodyPartExamined = 'CHEST'
    ds.ViewPosition = 'AP'
    ds.ViewCodeSequence = sequence(code('R-10206', 'SRT', 'antero-posterior'))
    ds.AnatomicRegionSequence = sequence(code('T-D3000', 'SRT', 'Chest'))
    ds.DistanceSourceToPatient, ds.DistanceSourceToDetector = '1000', '1100'
    ds.PatientOrientation = ['L', 'F']
    attach_pixels(ds, sop.pixel_format, sop.frames)


def build_mg(ds, sop, uids):
    _dx_common(ds, sop, uids, 'MG')
    ds.BurnedInAnnotation = 'NO'
    ds.BodyPartExamined = 'BREAST'
    ds.ImageLaterality = 'R'
    ds.ViewPosition = 'CC'
    ds.ViewCodeSequence = sequence(code('R-10242', 'SRT', 'cranio-caudal'))
    ds.ViewCodeSequence[0].ViewModifierCodeSequence = sequence()
    ds.OrganExposed = 'BREAST'
    ds.AnatomicRegionSequence = sequence(code('T-04000', 'SRT', 'Breast'))
    ds.PositionerType = 'MAMMOGRAPHIC'
    ds.PositionerPrimaryAngle = '0'
    ds.DistanceSourceToPatient, ds.DistanceSourceToDetector = '600', '650'
    ds.BodyPartThickness, ds.CompressionForce = '50', '100'
    ds.OrganDose, ds.EntranceDoseInmGy = '1', '5'
    ds.AnodeTargetMaterial, ds.FilterMaterial = 'MOLYBDENUM', 'MOLYBDENUM'
    ds.DetectorType = 'DIRECT'
    ds.PatientOrientation = ['A', 'R']
    ds.ImageType = ['ORIGINAL', 'PRIMARY', '']
    ds.BreastImplantPresent = 'NO'
    ds.PartialView = 'NO'
    ds.EstimatedRadiographicMagnificationFactor = '1'
    attach_pixels(ds, sop.pixel_format, sop.frames)


def build_io(ds, sop, uids):
    _dx_common(ds, sop, uids, 'IO')
    ds.PositionerType = 'NONE'
    ds.BodyPartExamined = 'JAW'
    ds.ImageLaterality = 'R'
    ds.AnatomicRegionSequence = sequence(code('T-D1217', 'SRT', 'Jaw region'))
    ds.AnatomicRegionSequence[0].AnatomicRegionModifierSequence = sequence(code('G-A101', 'SRT', 'Left'))
    ds.DetectorType = 'SCINTILLATOR'
    attach_pixels(ds, sop.pixel_format, sop.frames)


def build_cr(ds, sop, uids):
    common(ds, sop, uids, 'CR')
    ds.BodyPartExamined, ds.ViewPosition = 'CHEST', 'AP'
    general_image(ds, ('ORIGINAL', 'PRIMARY'))
    ds.ImagerPixelSpacing = ['0.1', '0.1']
    ds.KVP = '80'
    ds.ContentDate, ds.ContentTime = DATE, TIME
    rescale(ds)
    voi(ds, sop.pixel_format)
    attach_pixels(ds, sop.pixel_format, sop.frames)


def build_sc(ds, sop, uids):
    common(ds, sop, uids, 'OT')
    legacy_laterality(ds)
    ds.ConversionType = 'WSD'
    ds.PatientOrientation = ['L', 'F']
    attach_pixels(ds, sop.pixel_format, sop.frames)


def build_sc_multiframe(ds, sop, uids):
    common(ds, sop, uids, 'OT')
    legacy_laterality(ds)
    ds.ConversionType = 'WSD'
    ds.PatientOrientation = ['L', 'F']
    ds.ImageType = ['DERIVED', 'SECONDARY']
    ds.BurnedInAnnotation, ds.RecognizableVisualFeatures = 'NO', 'NO'
    ds.FrameIncrementPointer = (0x0018, 0x2002)
    ds.FrameLabelVector = ['F1', 'F2', 'F3']
    if sop.pixel_format in ('mono8', 'mono16u'):
        ds.PresentationLUTShape = 'IDENTITY'
        rescale(ds)
    attach_pixels(ds, sop.pixel_format, sop.frames)


def build_nm(ds, sop, uids):
    common(ds, sop, uids, 'NM')
    legacy_laterality(ds)
    ds.ImageType = ['ORIGINAL', 'PRIMARY', 'STATIC', 'EMISSION']
    ds.PatientOrientation = ['L', 'F']
    frames = sop.frames
    ds.ImageIndex = list(range(1, frames + 1))
    ds.NumberOfEnergyWindows = 1
    ds.EnergyWindowVector = [1] * frames
    ds.EnergyWindowInformationSequence = sequence(item(
        EnergyWindowRangeSequence=sequence(item(EnergyWindowLowerLimit='100', EnergyWindowUpperLimit='160'))))
    ds.NumberOfDetectors = 1
    ds.DetectorVector = [1] * frames
    ds.DetectorInformationSequence = sequence(item(
        CollimatorGridName='', CollimatorType='PARA', FieldOfViewShape='RECTANGLE',
        FieldOfViewDimensions=['10', '10'], FocalDistance=[], XFocusCenter=[], YFocusCenter=[],
        ZoomCenter=[], ZoomFactor=['1', '1'], CenterOfRotationOffset=[],
        GantryDetectorTilt=[], StartAngle=[], RadialPosition=[],
        ImageOrientationPatient=ORIENTATION, ImagePositionPatient=['0', '0', '0']))
    ds.RadiopharmaceuticalInformationSequence = sequence(item(
        RadionuclideCodeSequence=sequence(code('C-163A8', 'SRT', '^99m^Technetium')),
        RadionuclideTotalDose='100'))
    ds.PixelSpacing = ['1', '1']
    ds.FrameIncrementPointer = [(0x0054, 0x0010), (0x0054, 0x0020)]
    ds.CorrectedImage = ['UNIF']
    ds.CountsAccumulated = 0
    ds.ActualFrameDuration = 1000
    ds.PatientOrientationCodeSequence = sequence()
    ds.PatientGantryRelationshipCodeSequence = sequence()
    attach_pixels(ds, sop.pixel_format, sop.frames)


def build_nm_retired(ds, sop, uids):
    common(ds, sop, uids, 'NM')
    general_image(ds, ('ORIGINAL', 'PRIMARY'))
    attach_pixels(ds, sop.pixel_format, sop.frames)


def build_pet(ds, sop, uids):
    common(ds, sop, uids, 'PT')
    legacy_laterality(ds)
    frame_of_reference(ds, uids)
    ds.ImageType = ['ORIGINAL', 'PRIMARY']
    image_plane(ds)
    ds.SeriesDate, ds.SeriesTime = DATE, TIME
    ds.Units, ds.CountsSource, ds.SeriesType = 'BQML', 'EMISSION', ['STATIC', 'IMAGE']
    ds.CorrectedImage = ['DECY', 'ATTN', 'SCAT', 'DTIM', 'RAN', 'NORM']
    ds.DecayCorrection, ds.CollimatorType = 'START', 'NONE'
    ds.NumberOfSlices = 1
    ds.ImageIndex = 1
    ds.RescaleIntercept, ds.RescaleSlope = '0', '1'
    ds.AcquisitionDate, ds.AcquisitionTime = DATE, TIME
    ds.FrameReferenceTime, ds.ActualFrameDuration = '0', '1000'
    ds.SliceSensitivityFactor, ds.DecayFactor, ds.DoseCalibrationFactor = '1', '1', '1'
    ds.ScatterFractionFactor, ds.DeadTimeFactor = '0', '1'
    ds.RadiopharmaceuticalInformationSequence = sequence(item(
        RadiopharmaceuticalStartTime=TIME, RadionuclideTotalDose='100000000',
        RadionuclideHalfLife='6586', RadionuclidePositronFraction='0.97',
        RadionuclideCodeSequence=sequence(code('C-111A1', 'SRT', '^18^Fluorine'))))
    ds.PatientOrientationCodeSequence = sequence(item(
        CodeValue='F-10450', CodingSchemeDesignator='SRT', CodeMeaning='recumbent',
        PatientOrientationModifierCodeSequence=sequence(code('F-10340', 'SRT', 'supine'))))
    ds.PatientGantryRelationshipCodeSequence = sequence(code('F-10470', 'SRT', 'headfirst'))
    ds.PixelRepresentation = 1
    attach_pixels(ds, sop.pixel_format, sop.frames)


def build_rt_image(ds, sop, uids):
    common(ds, sop, uids, 'RTIMAGE')
    ds.OperatorsName = ''
    ds.ImageType = ['ORIGINAL', 'PRIMARY', 'PORTAL']
    ds.ConversionType = 'DI'
    ds.RTImageLabel = 'CORPUS'
    ds.ReportedValuesOrigin = 'ACTUAL'
    ds.RTImagePlane = 'NORMAL'
    ds.RadiationMachineName = 'CORPUS'
    ds.PrimaryDosimeterUnit = 'MU'
    ds.RadiationMachineSAD, ds.RTImageSID = '1000', '1500'
    ds.XRayImageReceptorAngle = '0'
    ds.PatientOrientation = ['L', 'F']
    ds.ImagePlanePixelSpacing = ['0.5', '0.5']
    ds.RTImagePosition = ['-10', '10']
    ds.GantryAngle, ds.BeamLimitingDeviceAngle, ds.PatientSupportAngle = '0', '0', '0'
    ds.PixelIntensityRelationshipSign = 1
    ds.PixelIntensityRelationship = 'LIN'
    ds.RescaleIntercept, ds.RescaleSlope, ds.RescaleType = '0', '1', 'US'
    ds.AcquisitionDate, ds.AcquisitionTime = DATE, TIME
    attach_pixels(ds, sop.pixel_format, sop.frames)


def build_rt_dose(ds, sop, uids):
    common(ds, sop, uids, 'RTDOSE')
    ds.OperatorsName = ''
    frame_of_reference(ds, uids)
    image_plane(ds)
    del ds.SliceThickness
    ds.DoseUnits, ds.DoseType, ds.DoseSummationType = 'GY', 'PHYSICAL', 'PLAN'
    ds.DoseGridScaling = '0.0001'
    ds.GridFrameOffsetVector = ['0', '1', '2']
    ds.FrameIncrementPointer = (0x3004, 0x000C)
    ds.ReferencedRTPlanSequence = sequence(item(
        ReferencedSOPClassUID='1.2.840.10008.5.1.4.1.1.481.5', ReferencedSOPInstanceUID=uids['related']))
    attach_pixels(ds, sop.pixel_format, sop.frames)


def _rt_series(ds, sop, uids, modality):
    common(ds, sop, uids, modality)
    ds.OperatorsName = ''


def build_rt_struct(ds, sop, uids):
    _rt_series(ds, sop, uids, 'RTSTRUCT')
    frame_of_reference(ds, uids)
    ds.StructureSetLabel = 'CORPUS'
    ds.StructureSetDate, ds.StructureSetTime = DATE, TIME
    ds.ReferencedFrameOfReferenceSequence = sequence(item(FrameOfReferenceUID=uids['frame']))
    ds.StructureSetROISequence = sequence(item(
        ROINumber=1, ReferencedFrameOfReferenceUID=uids['frame'], ROIName='CORPUS',
        ROIGenerationAlgorithm='MANUAL'))
    ds.ROIContourSequence = sequence(item(
        ReferencedROINumber=1, ROIDisplayColor=[255, 0, 0],
        ContourSequence=sequence(item(ContourGeometricType='POINT', NumberOfContourPoints=1,
                                      ContourData=['0', '0', '0']))))
    ds.RTROIObservationsSequence = sequence(item(
        ObservationNumber=1, ReferencedROINumber=1, RTROIInterpretedType='ORGAN', ROIInterpreter=''))


def _beam_limiting():
    return sequence(item(RTBeamLimitingDeviceType='X', NumberOfLeafJawPairs=1),
                    item(RTBeamLimitingDeviceType='Y', NumberOfLeafJawPairs=1))


def build_rt_plan(ds, sop, uids):
    _rt_series(ds, sop, uids, 'RTPLAN')
    ds.RTPlanLabel = 'CORPUS'
    ds.RTPlanDate, ds.RTPlanTime = DATE, TIME
    ds.RTPlanGeometry = 'PATIENT'
    ds.FrameOfReferenceUID = uids['frame']
    ds.PositionReferenceIndicator = ''
    ds.ReferencedStructureSetSequence = sequence(item(
        ReferencedSOPClassUID='1.2.840.10008.5.1.4.1.1.481.3', ReferencedSOPInstanceUID=uids['related']))
    ds.PatientSetupSequence = sequence(item(PatientSetupNumber=1, PatientPosition='HFS'))
    ds.FractionGroupSequence = sequence(item(
        FractionGroupNumber=1, NumberOfFractionsPlanned=1, NumberOfBeams=1, NumberOfBrachyApplicationSetups=0,
        ReferencedBeamSequence=sequence(item(ReferencedBeamNumber=1, BeamMeterset='100'))))
    ds.BeamSequence = sequence(item(
        BeamNumber=1, BeamName='CORPUS', BeamType='STATIC', RadiationType='PHOTON',
        TreatmentMachineName='CORPUS', PrimaryDosimeterUnit='MU', SourceAxisDistance='1000',
        BeamLimitingDeviceSequence=_beam_limiting(), TreatmentDeliveryType='TREATMENT',
        NumberOfWedges=0, NumberOfCompensators=0, NumberOfBoli=0, NumberOfBlocks=0,
        FinalCumulativeMetersetWeight='1', NumberOfControlPoints=2,
        ControlPointSequence=sequence(
            item(ControlPointIndex=0, NominalBeamEnergy='6', GantryAngle='0', GantryRotationDirection='NONE',
                 BeamLimitingDeviceAngle='0', BeamLimitingDeviceRotationDirection='NONE', PatientSupportAngle='0',
                 PatientSupportRotationDirection='NONE', TableTopEccentricAngle='0',
                 TableTopEccentricRotationDirection='NONE', TableTopVerticalPosition=[],
                 TableTopLongitudinalPosition=[], TableTopLateralPosition=[], IsocenterPosition=['0', '0', '0'],
                 CumulativeMetersetWeight='0',
                 BeamLimitingDevicePositionSequence=sequence(
                     item(RTBeamLimitingDeviceType='X', LeafJawPositions=['-50', '50']),
                     item(RTBeamLimitingDeviceType='Y', LeafJawPositions=['-50', '50']))),
            item(ControlPointIndex=1, CumulativeMetersetWeight='1'))))


def build_rt_ion_plan(ds, sop, uids):
    _rt_series(ds, sop, uids, 'RTPLAN')
    ds.RTPlanLabel = 'CORPUS'
    ds.RTPlanDate, ds.RTPlanTime = DATE, TIME
    ds.RTPlanGeometry = 'PATIENT'
    ds.FrameOfReferenceUID = uids['frame']
    ds.PositionReferenceIndicator = ''
    ds.ReferencedStructureSetSequence = sequence(item(
        ReferencedSOPClassUID='1.2.840.10008.5.1.4.1.1.481.3', ReferencedSOPInstanceUID=uids['related']))
    ds.PatientSetupSequence = sequence(item(PatientSetupNumber=1, PatientPosition='HFS'))
    ds.FractionGroupSequence = sequence(item(
        FractionGroupNumber=1, NumberOfFractionsPlanned=1, NumberOfBeams=1, NumberOfBrachyApplicationSetups=0,
        ReferencedBeamSequence=sequence(item(ReferencedBeamNumber=1, BeamMeterset='100'))))
    ds.IonBeamSequence = sequence(item(
        BeamNumber=1, BeamName='CORPUS', BeamType='STATIC', RadiationType='PROTON',
        TreatmentMachineName='CORPUS', PrimaryDosimeterUnit='MU', TreatmentDeliveryType='TREATMENT',
        ScanMode='NONE', VirtualSourceAxisDistances=[1000.0, 1000.0], NumberOfWedges=0, NumberOfCompensators=0,
        NumberOfBoli=0, NumberOfBlocks=0, NumberOfRangeShifters=0, NumberOfLateralSpreadingDevices=0,
        NumberOfRangeModulators=0, PatientSupportType='TABLE', FinalCumulativeMetersetWeight='1',
        NumberOfControlPoints=2,
        IonControlPointSequence=sequence(
            item(ControlPointIndex=0, NominalBeamEnergy='150', GantryAngle='0', GantryRotationDirection='NONE',
                 BeamLimitingDeviceAngle='0', BeamLimitingDeviceRotationDirection='NONE', PatientSupportAngle='0',
                 PatientSupportRotationDirection='NONE', TableTopVerticalPosition=[],
                 TableTopLongitudinalPosition=[], TableTopLateralPosition=[], IsocenterPosition=['0', '0', '0'],
                 CumulativeMetersetWeight='0'),
            item(ControlPointIndex=1, CumulativeMetersetWeight='1'))))


def _treatment_record(ds, sop, uids, modality):
    _rt_series(ds, sop, uids, modality)
    ds.TreatmentDate, ds.TreatmentTime = DATE, TIME
    ds.TreatmentMachineSequence = _machine()
    ds.ReferencedRTPlanSequence = sequence(item(
        ReferencedSOPClassUID='1.2.840.10008.5.1.4.1.1.481.5', ReferencedSOPInstanceUID=uids['related']))
    ds.PatientSetupSequence = sequence(item(PatientSetupNumber=1, PatientPosition='HFS'))
    ds.PatientPosition = 'HFS'
    ds.NumberOfFractionsPlanned = 1
    ds.PrimaryDosimeterUnit = 'MU'


def _machine():
    return sequence(item(TreatmentMachineName='CORPUS', Manufacturer='PACS_CORPUS', InstitutionName='',
                         InstitutionAddress='', InstitutionalDepartmentName='', ManufacturerModelName='',
                         DeviceSerialNumber=''))


def build_rt_beams_record(ds, sop, uids):
    _treatment_record(ds, sop, uids, 'RTRECORD')
    ds.CurrentFractionNumber = 1
    ds.TreatmentTerminationStatus = 'NORMAL'
    ds.TreatmentVerificationStatus = ''
    ds.TreatmentSessionBeamSequence = sequence(item(
        ReferencedBeamNumber=1, CurrentFractionNumber=1, TreatmentDeliveryType='TREATMENT',
        TreatmentTerminationStatus='NORMAL', TreatmentVerificationStatus='',
        BeamName='CORPUS', BeamType='STATIC', RadiationType='PHOTON', NumberOfWedges=0, NumberOfCompensators=0,
        NumberOfBoli=0, NumberOfBlocks=0, TreatmentMachineSequence=_machine(), PrimaryDosimeterUnit='MU',
        BeamLimitingDeviceLeafPairsSequence=sequence(
            item(RTBeamLimitingDeviceType='X', NumberOfLeafJawPairs=1)),
        NumberOfControlPoints=1,
        ControlPointDeliverySequence=sequence(item(
            ReferencedControlPointIndex=0, TreatmentControlPointDate=DATE, TreatmentControlPointTime=TIME,
            SpecifiedMeterset='100', DeliveredMeterset='100', NominalBeamEnergy='6', NominalBeamEnergyUnit='MV', DoseRateSet='600',
            GantryAngle='0', GantryRotationDirection='NONE', BeamLimitingDeviceAngle='0',
            BeamLimitingDeviceRotationDirection='NONE', PatientSupportAngle='0',
            PatientSupportRotationDirection='NONE', TableTopEccentricAngle='0',
            TableTopEccentricRotationDirection='NONE', TableTopVerticalPosition='0',
            TableTopLongitudinalPosition='0', TableTopLateralPosition='0', DoseRateDelivered='600',
            BeamLimitingDevicePositionSequence=sequence(
                item(RTBeamLimitingDeviceType='X', LeafJawPositions=['-50', '50'])))),
        CurrentTreatmentStatus='COMPLETED', CumulativeDoseToDoseReference=[]))


def build_rt_ion_record(ds, sop, uids):
    _treatment_record(ds, sop, uids, 'RTRECORD')
    ds.CurrentFractionNumber = 1
    ds.TreatmentTerminationStatus = 'NORMAL'
    ds.TreatmentVerificationStatus = ''
    ds.TreatmentSessionIonBeamSequence = sequence(item(
        ReferencedBeamNumber=1, CurrentFractionNumber=1, TreatmentDeliveryType='TREATMENT',
        TreatmentTerminationStatus='NORMAL', TreatmentVerificationStatus='', BeamName='CORPUS',
        BeamType='STATIC', RadiationType='PROTON', ScanMode='NONE', NumberOfWedges=0, NumberOfCompensators=0,
        NumberOfBoli=0, NumberOfBlocks=0, NumberOfRangeShifters=0, NumberOfLateralSpreadingDevices=0,
        NumberOfRangeModulators=0, PatientSupportType='TABLE', TreatmentMachineSequence=_machine(),
        PrimaryDosimeterUnit='MU', NumberOfControlPoints=1,
        IonControlPointDeliverySequence=sequence(item(
            ReferencedControlPointIndex=0, TreatmentControlPointDate=DATE, TreatmentControlPointTime=TIME,
            SpecifiedMeterset='100', DeliveredMeterset='100', NominalBeamEnergy='150', GantryAngle='0',
            GantryRotationDirection='NONE', BeamLimitingDeviceAngle='0',
            BeamLimitingDeviceRotationDirection='NONE', PatientSupportAngle='0',
            PatientSupportRotationDirection='NONE', TableTopVerticalPosition='0',
            TableTopLongitudinalPosition='0', TableTopLateralPosition='0')),
        CurrentTreatmentStatus='COMPLETED', CumulativeDoseToDoseReference=[]))


def build_rt_brachy_record(ds, sop, uids):
    _treatment_record(ds, sop, uids, 'RTRECORD')
    ds.CurrentFractionNumber = 1
    ds.TreatmentTerminationStatus = 'NORMAL'
    ds.TreatmentVerificationStatus = ''
    del ds.PrimaryDosimeterUnit
    ds.BrachyTreatmentTechnique, ds.BrachyTreatmentType = 'INTERSTITIAL', 'HDR'
    ds.RecordedSourceSequence = sequence(item(
        SourceNumber=1, SourceType='POINT', SourceManufacturer='', SourceSerialNumber='', SourceIsotopeName='Ir-192',
        SourceIsotopeHalfLife='73.83', ReferenceAirKermaRate='40000',
        SourceStrengthReferenceDate=DATE, SourceStrengthReferenceTime=TIME))
    ds.TreatmentSessionApplicationSetupSequence = sequence(item(
        ApplicationSetupType='MANUAL', ApplicationSetupNumber=1, TotalReferenceAirKerma='1',
        CurrentFractionNumber=1, TreatmentDeliveryType='TREATMENT', TreatmentTerminationStatus='NORMAL',
        TreatmentVerificationStatus='', CurrentTreatmentStatus='COMPLETED', ReferencedBrachyApplicationSetupNumber=1,
        RecordedChannelSequence=sequence(item(
            ChannelNumber=1, ChannelLength='100', TransferTubeNumber=None, ChannelTotalTime='1', SourceMovementType='STEPWISE',
            ReferencedSourceNumber=1, NumberOfControlPoints=1, SpecifiedChannelTotalTime='1',
            DeliveredChannelTotalTime='1',
            BrachyControlPointDeliveredSequence=sequence(
                item(ReferencedControlPointIndex=0, TreatmentControlPointDate=DATE, TreatmentControlPointTime=TIME,
                     ReleaseTime='0', CumulativeTimeWeight='0', ControlPointRelativePosition='0'),
                item(ReferencedControlPointIndex=1, TreatmentControlPointDate=DATE, TreatmentControlPointTime=TIME,
                     ReleaseTime='1', CumulativeTimeWeight='1', ControlPointRelativePosition='100'))))))


def build_rt_summary_record(ds, sop, uids):
    _rt_series(ds, sop, uids, 'RTRECORD')
    ds.CurrentTreatmentStatus = 'COMPLETED'
    ds.TreatmentDate, ds.TreatmentTime = DATE, TIME
    ds.FirstTreatmentDate, ds.MostRecentTreatmentDate = DATE, DATE
    ds.FractionGroupSummarySequence = sequence(item(
        FractionGroupType='EXTERNAL_BEAM', NumberOfFractionsPlanned=1, NumberOfFractionsDelivered=1,
        FractionStatusSummarySequence=sequence(item(ReferencedFractionNumber=1, TreatmentDate=DATE,
                                                    TreatmentTime=TIME, TreatmentTerminationStatus='NORMAL'))))
    ds.TreatmentSummaryCalculatedDoseReferenceSequence = sequence(item(
        DoseReferenceNumber=1, CumulativeDoseToDoseReference='1', DoseReferenceDescription='CORPUS'))
    ds.ReferencedRTPlanSequence = sequence(item(
        ReferencedSOPClassUID='1.2.840.10008.5.1.4.1.1.481.5', ReferencedSOPInstanceUID=uids['related']))


# SR, key object, waveform and encapsulated document IODs -------------------------------------

def _sr_document(ds, sop, uids, title, modality='SR'):
    common(ds, sop, uids, modality)
    ds.ValueType = 'CONTAINER'
    ds.ConceptNameCodeSequence = sequence(title)
    ds.ContinuityOfContent = 'SEPARATE'
    ds.CompletionFlag, ds.VerificationFlag = 'COMPLETE', 'UNVERIFIED'
    ds.PerformedProcedureCodeSequence = sequence()
    ds.ReferencedPerformedProcedureStepSequence = sequence()


def build_sr_basic(ds, sop, uids):
    _sr_document(ds, sop, uids, code('18748-4', 'LN', 'Diagnostic Imaging Report'))
    ds.ContentSequence = sequence(item(
        RelationshipType='CONTAINS', ValueType='TEXT', TextValue='CORPUS finding',
        ConceptNameCodeSequence=sequence(code('121071', 'DCM', 'Finding'))))


def build_sr(ds, sop, uids):
    build_sr_basic(ds, sop, uids)
    ds.ContentSequence[0].ContentSequence = sequence(item(
        RelationshipType='HAS CONCEPT MOD', ValueType='CODE',
        ConceptNameCodeSequence=sequence(code('121049', 'DCM', 'Language of Content Item and Descendants')),
        ConceptCodeSequence=sequence(code('en', 'RFC5646', 'English'))))


def build_sr_cad(ds, sop, uids):
    title = {'.50': code('111036', 'DCM', 'Mammography CAD Report'),
             '.65': code('111037', 'DCM', 'Chest CAD Report'),
             '.69': code('112000', 'DCM', 'Colon CAD Report')}[sop.uid[-3:]]
    _sr_document(ds, sop, uids, title)
    ds.ContentSequence = sequence(item(
        RelationshipType='CONTAINS', ValueType='CODE',
        ConceptNameCodeSequence=sequence(code('111001', 'DCM', 'Algorithm Name')),
        ConceptCodeSequence=sequence(code('111222', 'DCM', 'Succeeded'))))


def build_sr_dose(ds, sop, uids):
    _sr_document(ds, sop, uids, code('113701', 'DCM', 'X-Ray Radiation Dose Report'))
    ds.ContentSequence = sequence(item(
        RelationshipType='HAS CONCEPT MOD', ValueType='CODE',
        ConceptNameCodeSequence=sequence(code('121058', 'DCM', 'Procedure reported')),
        ConceptCodeSequence=sequence(code('P5-08000', 'SRT', 'Mammography'))))


def build_sr_waveform_annotation(ds, sop, uids):
    _sr_document(ds, sop, uids, code('18748-4', 'LN', 'Diagnostic Imaging Report'))
    ds.ReferencedRequestSequence = sequence()
    ds.ContentSequence = sequence(item(
        RelationshipType='CONTAINS', ValueType='TEXT', TextValue='CORPUS annotation',
        ConceptNameCodeSequence=sequence(code('121071', 'DCM', 'Finding'))))


def build_kos(ds, sop, uids):
    common(ds, sop, uids, 'KO')
    ds.ReferencedPerformedProcedureStepSequence = sequence()
    ds.CurrentRequestedProcedureEvidenceSequence = sequence(item(
        StudyInstanceUID=uids['study'],
        ReferencedSeriesSequence=sequence(item(
            SeriesInstanceUID=uids['series'],
            ReferencedSOPSequence=sequence(item(
                ReferencedSOPClassUID='1.2.840.10008.5.1.4.1.1.2', ReferencedSOPInstanceUID=uids['related']))))))
    ds.ValueType = 'CONTAINER'
    ds.ConceptNameCodeSequence = sequence(code('113000', 'DCM', 'Of Interest'))
    ds.ContinuityOfContent = 'SEPARATE'
    ds.ContentTemplateSequence = sequence(item(MappingResource='DCMR', TemplateIdentifier='2010'))
    ds.ContentSequence = sequence(item(
        RelationshipType='CONTAINS', ValueType='IMAGE',
        ReferencedSOPSequence=sequence(item(
            ReferencedSOPClassUID='1.2.840.10008.5.1.4.1.1.2', ReferencedSOPInstanceUID=uids['related']))))


WAVEFORMS = {
    # family: (modality, channels, samples, frequency, source code)
    'waveform-ecg12': ('ECG', 12, 16, 500, ('MDC_ECG_LEAD_I', 'MDC', 'Lead I (Einthoven)')),
    'waveform-ecg': ('ECG', 2, 16, 500, ('MDC_ECG_LEAD_I', 'MDC', 'Lead I (Einthoven)')),
    'waveform-ambulatory': ('ECG', 2, 16, 100, ('MDC_ECG_LEAD_I', 'MDC', 'Lead I (Einthoven)')),
    'waveform-hemodynamic': ('HD', 2, 16, 250, ('2:1', 'SCPECG', 'Lead I')),
    'waveform-ep': ('EPS', 2, 16, 1000, ('2:1', 'SCPECG', 'Lead I')),
    'waveform-voice': ('AU', 1, 8, 8000, ('109110', 'DCM', 'Voice')),
    'waveform-audio': ('AU', 2, 16, 44100, ('109110', 'DCM', 'Voice')),
    'waveform-arterial': ('HD', 1, 16, 250, ('2:1', 'SCPECG', 'Lead I')),
    'waveform-respiratory': ('RESP', 1, 16, 100, ('2:1', 'SCPECG', 'Lead I')),
    'waveform-respiratory-multi': ('RESP', 2, 16, 100, ('2:1', 'SCPECG', 'Lead I')),
    'waveform-eeg': ('EEG', 2, 16, 250, ('2:1', 'SCPECG', 'Lead I')),
    'waveform-emg': ('EMG', 2, 16, 1000, ('2:1', 'SCPECG', 'Lead I')),
    'waveform-eog': ('EOG', 2, 16, 250, ('2:1', 'SCPECG', 'Lead I')),
    'waveform-sleep': ('EEG', 2, 16, 250, ('2:1', 'SCPECG', 'Lead I')),
    'waveform-body': ('BP', 1, 16, 100, ('2:1', 'SCPECG', 'Lead I')),
}


def build_waveform(ds, sop, uids):
    modality, channels, bits, frequency, source = WAVEFORMS[sop.family]
    common(ds, sop, uids, modality)
    if sop.family == 'waveform-hemodynamic':
        legacy_laterality(ds)
    ds.AcquisitionDateTime = DATETIME
    ds.AcquisitionContextSequence = sequence()
    ds.SynchronizationFrameOfReferenceUID = uids['frame']
    ds.SynchronizationTrigger = 'NO TRIGGER'
    ds.AcquisitionTimeSynchronized = 'N'
    samples = 8
    values = np.arange(channels * samples, dtype='<i2' if bits == 16 else 'u1') * (3 if bits == 16 else 1)
    channel_items = [item(
        ChannelSourceSequence=sequence(code(*source)), WaveformChannelNumber=index + 1,
        ChannelSensitivity='1', ChannelSensitivityUnitsSequence=sequence(code('uV', 'UCUM', 'microvolt')),
        ChannelSensitivityCorrectionFactor='1', ChannelBaseline='0', ChannelSampleSkew='0',
        WaveformBitsStored=bits) for index in range(channels)]
    ds.WaveformSequence = sequence(item(
        WaveformOriginality='ORIGINAL', NumberOfWaveformChannels=channels,
        NumberOfWaveformSamples=samples, SamplingFrequency=str(frequency),
        ChannelDefinitionSequence=sequence(*channel_items), WaveformBitsAllocated=bits,
        WaveformSampleInterpretation='SS' if bits == 16 else 'UB', WaveformData=values.tobytes()))
    ds.WaveformSequence[0]['WaveformData'].VR = 'OW' if bits == 16 else 'OB'


def build_waveform_ps(ds, sop, uids):
    common(ds, sop, uids, 'PR')
    ds.ContentLabel, ds.ContentDescription = 'CORPUS', 'Waveform presentation'
    ds.ContentCreatorName = ''
    ds.PresentationCreationDate, ds.PresentationCreationTime = DATE, TIME
    ds.ReferencedSeriesSequence = sequence(item(
        SeriesInstanceUID=uids['series'],
        ReferencedInstanceSequence=sequence(item(
            ReferencedSOPClassUID='1.2.840.10008.5.1.4.1.1.9.1.1', ReferencedSOPInstanceUID=uids['related']))))


def build_encapsulated(ds, sop, uids):
    common(ds, sop, uids, 'DOC')
    ds.ConversionType = 'WSD'
    ds.BurnedInAnnotation = 'NO'
    ds.DocumentTitle = 'CORPUS'
    ds.ConceptNameCodeSequence = sequence()
    ds.AcquisitionDateTime = DATETIME
    if sop.family == 'encapsulated-pdf':
        ds.MIMETypeOfEncapsulatedDocument = 'application/pdf'
        document = PDF
    else:
        ds.MIMETypeOfEncapsulatedDocument = 'text/XML'
        document = CDA
        ds.HL7InstanceIdentifier = '1.2.3^4'
    ds.EncapsulatedDocument = document + (b'\0' if len(document) % 2 else b'')
    ds.EncapsulatedDocumentLength = len(document)


# Multi-frame functional-group IODs ------------------------------------------------------

def _frame_content(index, dimensions=True, temporal=False):
    content = item(FrameAcquisitionDateTime=DATETIME, FrameReferenceDateTime=DATETIME,
                   FrameAcquisitionDuration=1.0, StackID='1', InStackPositionNumber=index + 1)
    if dimensions:
        content.DimensionIndexValues = [index + 1]
    if temporal:
        content.TemporalPositionIndex = 1
    return content


def _dimensions(ds, uids, pointer=(0x0020, 0x9057), functional=(0x0020, 0x9111)):
    organization = uids['dimension']
    ds.DimensionOrganizationSequence = sequence(item(DimensionOrganizationUID=organization))
    ds.DimensionIndexSequence = sequence(item(
        DimensionIndexPointer=pointer, FunctionalGroupPointer=functional, DimensionOrganizationUID=organization))


def _plane(index):
    return dict(PlanePositionSequence=sequence(item(ImagePositionPatient=['0', '0', str(index)])))


def _measures():
    return sequence(item(PixelSpacing=['1', '1'], SliceThickness='1', SpacingBetweenSlices='1'))


def _per_frame(frames, extra=None, dimensions=True, temporal=False):
    groups = []
    for index in range(frames):
        group = item(FrameContentSequence=sequence(_frame_content(index, dimensions, temporal)), **_plane(index))
        for key, value in (extra(index) if extra else {}).items():
            setattr(group, key, value)
        groups.append(group)
    return Sequence(groups)


def _shared(**extra):
    return sequence(item(PixelMeasuresSequence=_measures(),
                         PlaneOrientationSequence=sequence(item(ImageOrientationPatient=ORIENTATION)), **extra))


def _enhanced_common(ds, sop, uids, modality):
    common(ds, sop, uids, modality)
    enhanced_equipment(ds)
    frame_of_reference(ds, uids)
    _dimensions(ds, uids)
    ds.AcquisitionContextSequence = sequence()
    ds.InstanceNumber = 1
    ds.AcquisitionNumber = 1
    ds.AcquisitionDateTime = DATETIME
    ds.AcquisitionDuration = 1.0
    ds.ContentQualification = 'RESEARCH'
    ds.BurnedInAnnotation, ds.RecognizableVisualFeatures = 'NO', 'NO'
    ds.LossyImageCompression = '00'
    ds.PresentationLUTShape = 'IDENTITY'
    ds.PixelPresentation = 'MONOCHROME'
    ds.VolumetricProperties, ds.VolumeBasedCalculationTechnique = 'VOLUME', 'NONE'
    ds.ContentDate, ds.ContentTime = DATE, TIME
    ds.NumberOfFrames = sop.frames
    ds.ImageType = ['ORIGINAL', 'PRIMARY', 'VOLUME', 'NONE']


def build_enhanced_ct(ds, sop, uids):
    _enhanced_common(ds, sop, uids, 'CT')
    ds.PatientPosition = 'HFS'
    ds.SharedFunctionalGroupsSequence = _shared(
        IrradiationEventIdentificationSequence=sequence(item(IrradiationEventUID=uids['related'])),
        CTGeometrySequence=sequence(item(DistanceSourceToDetector='1000', DistanceSourceToDataCollectionCenter=500.0)),
        CTImageFrameTypeSequence=sequence(item(
            FrameType=['ORIGINAL', 'PRIMARY', 'VOLUME', 'NONE'], PixelPresentation='MONOCHROME',
            VolumetricProperties='VOLUME', VolumeBasedCalculationTechnique='NONE')),
        CTAcquisitionTypeSequence=sequence(item(AcquisitionType='SEQUENCED', ConstantVolumeFlag='NO',
                                                FluoroscopyFlag='NO')),
        CTAcquisitionDetailsSequence=sequence(item(RotationDirection='CW', RevolutionTime=1.0,
                                                   SingleCollimationWidth=1.0, TotalCollimationWidth=1.0,
                                                   TableHeight='100', GantryDetectorTilt='0',
                                                   DataCollectionDiameter='200')),
        CTTableDynamicsSequence=sequence(item(TableSpeed=0.0, TableFeedPerRotation=0.0, SpiralPitchFactor=0.0)),
        CTPositionSequence=sequence(item(TablePosition=0.0, ReconstructionTargetCenterPatient=[0.0, 0.0, 0.0],
                                         DataCollectionCenterPatient=[0.0, 0.0, 0.0])),
        CTReconstructionSequence=sequence(item(ReconstructionAlgorithm='FILTER_BACK_PROJ', ConvolutionKernel='B',
                                               ConvolutionKernelGroup='SOFT', ReconstructionAngle=360.0,
                                               ImageFilter='NONE', ReconstructionDiameter='200',
                                               ReconstructionPixelSpacing=[1.0, 1.0])),
        CTExposureSequence=sequence(item(ExposureTimeInms=100.0, XRayTubeCurrentInmA=100.0, ExposureInmAs=10.0,
                                         ExposureModulationType='NONE')),
        CTXRayDetailsSequence=sequence(item(KVP='120', FocalSpots=['1'], FilterType='FLAT', FilterMaterial='ALUMINUM')),
        PixelValueTransformationSequence=sequence(item(RescaleIntercept='-1024', RescaleSlope='1', RescaleType='HU')),
        FrameAnatomySequence=sequence(item(FrameLaterality='U', AnatomicRegionSequence=sequence(
            code('T-D3000', 'SRT', 'Chest')))),
        FrameVOILUTSequence=sequence(item(WindowCenter='0', WindowWidth='4096')))
    ds.PerFrameFunctionalGroupsSequence = _per_frame(sop.frames)
    attach_pixels(ds, sop.pixel_format, sop.frames)


def build_legacy_ct(ds, sop, uids):
    common(ds, sop, uids, 'CT')
    enhanced_equipment(ds)
    ds.AcquisitionContextSequence = sequence()
    ds.PixelPresentation, ds.VolumetricProperties, ds.VolumeBasedCalculationTechnique = 'MONOCHROME', 'VOLUME', 'NONE'
    frame_of_reference(ds, uids)
    ds.NumberOfFrames = sop.frames
    ds.ImageType = ['ORIGINAL', 'PRIMARY', 'VOLUME', 'NONE']
    ds.BurnedInAnnotation, ds.RecognizableVisualFeatures = 'NO', 'NO'
    ds.LossyImageCompression = '00'
    ds.AcquisitionNumber = 1
    ds.AcquisitionDateTime = DATETIME
    ds.PresentationLUTShape = 'IDENTITY'
    ds.ContentQualification = 'RESEARCH'
    ds.SharedFunctionalGroupsSequence = _shared(
        CTImageFrameTypeSequence=sequence(item(FrameType=['ORIGINAL', 'PRIMARY', 'VOLUME', 'NONE'],
                                                  PixelPresentation='MONOCHROME', VolumetricProperties='VOLUME',
                                                  VolumeBasedCalculationTechnique='NONE')),
        PixelValueTransformationSequence=sequence(item(RescaleIntercept='-1024', RescaleSlope='1', RescaleType='HU')),
        FrameVOILUTSequence=sequence(item(WindowCenter='0', WindowWidth='4096')),
        FrameAnatomySequence=sequence(item(FrameLaterality='U', AnatomicRegionSequence=sequence(
            code('T-D3000', 'SRT', 'Chest')))),
        UnassignedSharedConvertedAttributesSequence=sequence(item(KVP='120')))
    ds.PerFrameFunctionalGroupsSequence = _per_frame(
        sop.frames, lambda index: dict(
            ConversionSourceAttributesSequence=sequence(item(
                ReferencedSOPClassUID='1.2.840.10008.5.1.4.1.1.2', ReferencedSOPInstanceUID=uids['related'] + f'.{index + 1}')),
            UnassignedPerFrameConvertedAttributesSequence=sequence(item(SliceLocation=str(index)))),
        dimensions=False)
    attach_pixels(ds, sop.pixel_format, sop.frames)


def build_enhanced_mr(ds, sop, uids):
    _enhanced_common(ds, sop, uids, 'MR')
    ds.PatientPosition = 'HFS'
    ds.KSpaceFiltering, ds.ComplexImageComponent, ds.OversamplingPhase = 'NONE', 'MAGNITUDE', 'NONE'
    ds.ImageType = ['ORIGINAL', 'PRIMARY', 'T1', 'NONE']
    ds.ResonantNucleus = '1H'
    ds.MagneticFieldStrength = 3.0
    ds.ImagingFrequency = 123.0
    ds.AcquisitionContrast = 'T1'
    ds.ApplicableSafetyStandardAgency = 'IEC'
    ds.MRAcquisitionType = '2D'
    ds.PulseSequenceName = 'CORPUS'
    ds.EchoPulseSequence, ds.MultipleSpinEcho = 'SPIN', 'NO'
    ds.PhaseContrast, ds.TimeOfFlightContrast = 'NO', 'NO'
    ds.SteadyStatePulseSequence, ds.EchoPlanarPulseSequence = 'NONE', 'NO'
    ds.SaturationRecovery, ds.SpectrallySelectedSuppression = 'NO', 'NONE'
    ds.GeometryOfKSpaceTraversal, ds.RectilinearPhaseEncodeReordering = 'RECTILINEAR', 'LINEAR'
    ds.SegmentedKSpaceTraversal = 'SINGLE'
    ds.NumberOfKSpaceTrajectories = 1
    ds.MultiPlanarExcitation = 'NO'
    ds.SharedFunctionalGroupsSequence = _shared(
        MRImageFrameTypeSequence=sequence(item(
            FrameType=['ORIGINAL', 'PRIMARY', 'T1', 'NONE'], PixelPresentation='MONOCHROME',
            VolumetricProperties='VOLUME', VolumeBasedCalculationTechnique='NONE',
            ComplexImageComponent='MAGNITUDE', AcquisitionContrast='T1')),
        MRTimingAndRelatedParametersSequence=sequence(item(RepetitionTime=500.0, FlipAngle=90.0, EchoTrainLength=1,
                                                           RFEchoTrainLength=1, GradientEchoTrainLength=0,
                                                           SpecificAbsorptionRateSequence=sequence(item(
                                                               SpecificAbsorptionRateDefinition='IEC_WHOLE_BODY', SpecificAbsorptionRateValue=1.0)),
                                                           OperatingModeSequence=sequence(item(
                                                               OperatingModeType='STATIC FIELD', OperatingMode='IEC_NORMAL')))),
        MRFOVGeometrySequence=sequence(item(InPlanePhaseEncodingDirection='COLUMN', MRAcquisitionFrequencyEncodingSteps=48,
                                            MRAcquisitionPhaseEncodingStepsInPlane=33, PercentSampling=100.0,
                                            PercentPhaseFieldOfView=100.0)),
        MREchoSequence=sequence(item(EffectiveEchoTime=10.0)),
        MRModifierSequence=sequence(item(InversionRecovery='NO', FlowCompensation='NONE', T2Preparation='NO',
                                         SpectrallySelectedExcitation='NONE', SpatialPresaturation='NONE',
                                         PartialFourier='NO', ParallelAcquisition='NO')),
        MRImagingModifierSequence=sequence(item(MagnetizationTransfer='NONE', BloodSignalNulling='NO', Tagging='NONE',
                                                TransmitterFrequency=123.0, PixelBandwidth=100.0)),
        MRReceiveCoilSequence=sequence(item(ReceiveCoilName='CORPUS', ReceiveCoilManufacturerName='',
                                            ReceiveCoilType='BODY', QuadratureReceiveCoil='NO')),
        MRTransmitCoilSequence=sequence(item(TransmitCoilName='CORPUS', TransmitCoilManufacturerName='',
                                             TransmitCoilType='BODY')),
        MRAveragesSequence=sequence(item(NumberOfAverages=1.0)),
        PixelValueTransformationSequence=sequence(item(RescaleIntercept='0', RescaleSlope='1', RescaleType='US')),
        FrameAnatomySequence=sequence(item(FrameLaterality='U', AnatomicRegionSequence=sequence(
            code('T-D3000', 'SRT', 'Chest')))),
        FrameVOILUTSequence=sequence(item(WindowCenter='32768', WindowWidth='65536')))
    ds.PerFrameFunctionalGroupsSequence = _per_frame(sop.frames)
    attach_pixels(ds, sop.pixel_format, sop.frames)


def build_enhanced_xa(ds, sop, uids):
    common(ds, sop, uids, 'XA')
    enhanced_equipment(ds)
    frame_of_reference(ds, uids)
    ds.AcquisitionContextSequence = sequence()
    ds.NumberOfFrames = sop.frames
    ds.ImageType = ['ORIGINAL', 'PRIMARY', 'SINGLE PLANE', 'NONE']
    _dimensions(ds, uids)
    ds.AcquisitionNumber = 1
    ds.AcquisitionDateTime = DATETIME
    ds.AcquisitionDuration = 1.0
    ds.ContentQualification = 'RESEARCH'
    ds.BurnedInAnnotation, ds.RecognizableVisualFeatures = 'NO', 'NO'
    ds.LossyImageCompression = '00'
    ds.PresentationLUTShape = 'IDENTITY'
    ds.PixelIntensityRelationship = 'LIN'
    ds.CArmPositionerTabletopRelationship = 'NO'
    ds.AcquisitionProtocolName = 'CORPUS'
    ds.AcquiredImageAreaDoseProduct = 1.0
    ds.KVP = '80'
    ds.AveragePulseWidth, ds.RadiationMode = '10', 'CONTINUOUS'
    ds.XRayTubeCurrentInmA, ds.ExposureTimeInms, ds.ExposureInmAs = 100.0, 10.0, 1.0
    ds.DistanceReceptorPlaneToDetectorHousing = None
    ds.DistanceSourceToDetector = '1000'
    ds.DistanceSourceToIsocenter = 700.0
    ds.RadiationSetting = 'GR'
    ds.PositionerType = 'CARM'
    ds.PlanesInAcquisition, ds.PlaneIdentification = 'MONOPLANE', 'MONOPLANE'
    ds.SharedFunctionalGroupsSequence = sequence(item(
        XAXRFFrameCharacteristicsSequence=sequence(item(DerivationDescription='')),
        FrameAnatomySequence=sequence(item(FrameLaterality='U', AnatomicRegionSequence=sequence(
            code('T-D3000', 'SRT', 'Chest')))),
        FramePixelShiftSequence=sequence(item(SubtractionItemID=1, MaskSubPixelShift=[0.0, 0.0])),
        FrameVOILUTSequence=sequence(item(WindowCenter='2048', WindowWidth='4096')),
        IrradiationEventIdentificationSequence=sequence(item(IrradiationEventUID=uids['related'])),
        FramePixelDataPropertiesSequence=sequence(item(
            FrameType=['ORIGINAL', 'PRIMARY', 'SINGLE PLANE', 'NONE'], PixelIntensityRelationship='LIN',
            ImagerPixelSpacing=['0.1', '0.1'], GeometricalProperties='NON_UNIFORM', PixelIntensityRelationshipSign=1,
            GeometricMaximumDistortion=0.0, ImageProcessingApplied='NONE')),
        PositionerPositionSequence=sequence(item(PositionerPrimaryAngle='0', PositionerSecondaryAngle='0')),
        TablePositionSequence=sequence(item(TableTopVerticalPosition=0.0, TableTopLongitudinalPosition=0.0,
                                            TableTopLateralPosition=0.0, TableHorizontalRotationAngle=0.0,
                                            TableHeadTiltAngle=0.0, TableCradleTiltAngle=0.0)),
        CollimatorShapeSequence=sequence(item(
            CollimatorShape=['RECTANGULAR'], CollimatorLeftVerticalEdge=0, CollimatorRightVerticalEdge=47,
            CollimatorUpperHorizontalEdge=0, CollimatorLowerHorizontalEdge=32)),
        PixelIntensityRelationshipLUTSequence=sequence(item(LUTFunction='TO_LOG', LUTDescriptor=[2, 0, 16],
                                                            LUTData=np.array([0, 1], dtype='<u2').tobytes()))))
    ds.PerFrameFunctionalGroupsSequence = Sequence(
        [item(FrameContentSequence=sequence(_frame_content(index))) for index in range(sop.frames)])
    attach_pixels(ds, sop.pixel_format, sop.frames)


def build_xray3d(ds, sop, uids):
    common(ds, sop, uids, 'XA' if sop.uid.endswith('13.1.1') else 'DX')
    enhanced_equipment(ds)
    ds.AcquisitionContextSequence = sequence()
    ds.PixelPresentation, ds.VolumetricProperties, ds.VolumeBasedCalculationTechnique = 'MONOCHROME', 'VOLUME', 'NONE'
    frame_of_reference(ds, uids)
    ds.NumberOfFrames = sop.frames
    ds.ImageType = ['ORIGINAL', 'PRIMARY', 'VOLUME', 'NONE']
    _dimensions(ds, uids)
    ds.AcquisitionDateTime = DATETIME
    ds.AcquisitionDuration = 1.0
    ds.ContentQualification = 'RESEARCH'
    ds.BurnedInAnnotation, ds.RecognizableVisualFeatures = 'NO', 'NO'
    ds.LossyImageCompression = '00'
    ds.PresentationLUTShape = 'IDENTITY'
    ds.ContributingSourcesSequence = sequence(item(AcquisitionDateTime=DATETIME, Manufacturer='', Rows=33, Columns=48,
                                                   BitsStored=12, LossyImageCompression='00'))
    ds.ReconstructionDescription = 'CORPUS'
    ds.SharedFunctionalGroupsSequence = _shared(
        XRay3DFrameTypeSequence=sequence(item(FrameType=['ORIGINAL', 'PRIMARY', 'VOLUME', 'NONE'],
                                              PixelPresentation='MONOCHROME', VolumetricProperties='VOLUME',
                                              VolumeBasedCalculationTechnique='NONE')),
        PixelValueTransformationSequence=sequence(item(RescaleIntercept='0', RescaleSlope='1', RescaleType='US')),
        FrameAnatomySequence=sequence(item(FrameLaterality='U', AnatomicRegionSequence=sequence(
            code('T-D3000', 'SRT', 'Chest')))),
        FrameVOILUTSequence=sequence(item(WindowCenter='2048', WindowWidth='4096')))
    ds.PerFrameFunctionalGroupsSequence = _per_frame(sop.frames)
    attach_pixels(ds, sop.pixel_format, sop.frames)


def _radiopharmaceutical():
    return sequence(item(
        RadiopharmaceuticalAgentNumber=1, RadionuclideCodeSequence=sequence(code('C-111A1', 'SRT', '^18^Fluorine')),
        RadiopharmaceuticalCodeSequence=sequence(code('C-B1031', 'SRT', 'Fluorodeoxyglucose F^18^')),
        AdministrationRouteCodeSequence=sequence(code('G-D101', 'SRT', 'Intravenous route')),
        RadionuclidePositronFraction='0.97', RadionuclideHalfLife='6586', RadionuclideTotalDose='100000000',
        RadiopharmaceuticalStartDateTime=DATETIME))


def build_enhanced_pet(ds, sop, uids):
    common(ds, sop, uids, 'PT')
    enhanced_equipment(ds)
    ds.AcquisitionContextSequence = sequence()
    frame_of_reference(ds, uids)
    ds.NumberOfFrames = sop.frames
    ds.ImageType = ['ORIGINAL', 'PRIMARY', 'STATIC', 'NONE']
    _dimensions(ds, uids)
    ds.AcquisitionNumber = 1
    ds.AcquisitionDateTime = DATETIME
    ds.AcquisitionDuration = 1.0
    ds.ContentQualification = 'RESEARCH'
    ds.BurnedInAnnotation, ds.RecognizableVisualFeatures = 'NO', 'NO'
    ds.LossyImageCompression = '00'
    ds.PresentationLUTShape = 'IDENTITY'
    ds.Units, ds.CountsSource = 'BQML', 'EMISSION'
    ds.PatientOrientationCodeSequence = sequence(item(
        CodeValue='F-10450', CodingSchemeDesignator='SRT', CodeMeaning='recumbent',
        PatientOrientationModifierCodeSequence=sequence(code('F-10340', 'SRT', 'supine'))))
    ds.PatientGantryRelationshipCodeSequence = sequence(code('F-10470', 'SRT', 'headfirst'))
    ds.PixelPresentation, ds.VolumetricProperties, ds.VolumeBasedCalculationTechnique = 'MONOCHROME', 'VOLUME', 'NONE'
    for correction in ('DecayCorrected', 'AttenuationCorrected', 'ScatterCorrected', 'DeadTimeCorrected',
                       'GantryMotionCorrected', 'PatientMotionCorrected', 'CountLossNormalizationCorrected',
                       'RandomsCorrected', 'NonUniformRadialSamplingCorrected', 'SensitivityCalibrated',
                       'DetectorNormalizationCorrection'):
        setattr(ds, correction, 'NO')
    ds.AcquisitionStartCondition, ds.AcquisitionTerminationCondition = 'MANU', 'MANU'
    ds.TypeOfDetectorMotion, ds.CollimatorType = 'NONE', 'NONE'
    ds.TransverseDetectorSeparation, ds.AxialDetectorDimension = 800.0, 200.0
    ds.CoincidenceWindowWidth = '4'
    ds.EnergyWindowRangeSequence = sequence(item(EnergyWindowLowerLimit='425', EnergyWindowUpperLimit='650'))
    ds.TableMotion, ds.TimeOfFlightInformationUsed = 'STATIC', 'FALSE'
    ds.ViewCodeSequence = sequence(code('G-A117', 'SRT', 'Transverse'))
    ds.SliceProgressionDirection = 'CAUDO_CRANIAL'
    ds.SharedFunctionalGroupsSequence = _shared(
        PETFrameTypeSequence=sequence(item(FrameType=['ORIGINAL', 'PRIMARY', 'STATIC', 'NONE'],
                                           PixelPresentation='MONOCHROME', VolumetricProperties='VOLUME',
                                           VolumeBasedCalculationTechnique='NONE')),
        RadiopharmaceuticalUsageSequence=sequence(item(RadiopharmaceuticalAgentNumber=1)),
        PETFrameAcquisitionSequence=sequence(item(TableHeight=100.0, GantryDetectorTilt=0.0, GantryDetectorSlew=0.0,
                                                  DataType='TISSUE', DataCollectionDiameter='200')),
        PETDetectorMotionDetailsSequence=sequence(item(RotationDirection='CW', RevolutionTime=1.0)),
        PETPositionSequence=sequence(item(TablePosition=0.0, DataCollectionCenterPatient=[0.0, 0.0, 0.0],
                                          ReconstructionTargetCenterPatient=[0.0, 0.0, 0.0])),
        PETFrameCorrectionFactorsSequence=sequence(item(PrimaryPromptsCountsAccumulated=1, SliceSensitivityFactor='1',
                                                        ScatterFractionFactor='0', DeadTimeFactor='1')),
        PETReconstructionSequence=sequence(item(ReconstructionAlgorithm='FBP', ReconstructionType='2D',
                                                IterativeReconstructionMethod='NO', ReconstructionDiameter='200',
                                                ReconstructionPixelSpacing=[1.0, 1.0])),
        PixelValueTransformationSequence=sequence(item(RescaleIntercept='0', RescaleSlope='1', RescaleType='BQML')),
        FrameAnatomySequence=sequence(item(FrameLaterality='U', AnatomicRegionSequence=sequence(
            code('T-D3000', 'SRT', 'Chest')))),
        FrameVOILUTSequence=sequence(item(WindowCenter='32768', WindowWidth='65536')))
    ds.PerFrameFunctionalGroupsSequence = _per_frame(sop.frames, temporal=True)
    ds.RadiopharmaceuticalInformationSequence = _radiopharmaceutical()
    attach_pixels(ds, sop.pixel_format, sop.frames)


def build_legacy_pet(ds, sop, uids):
    common(ds, sop, uids, 'PT')
    enhanced_equipment(ds)
    ds.AcquisitionContextSequence = sequence()
    ds.PixelPresentation, ds.VolumetricProperties, ds.VolumeBasedCalculationTechnique = 'MONOCHROME', 'VOLUME', 'NONE'
    frame_of_reference(ds, uids)
    ds.NumberOfFrames = sop.frames
    ds.ImageType = ['ORIGINAL', 'PRIMARY', 'STATIC', 'NONE']
    ds.BurnedInAnnotation, ds.RecognizableVisualFeatures = 'NO', 'NO'
    ds.LossyImageCompression = '00'
    ds.AcquisitionNumber = 1
    ds.AcquisitionDateTime = DATETIME
    ds.PresentationLUTShape = 'IDENTITY'
    ds.ContentQualification = 'RESEARCH'
    ds.Units, ds.CountsSource = 'BQML', 'EMISSION'
    ds.SharedFunctionalGroupsSequence = _shared(
        PETFrameTypeSequence=sequence(item(FrameType=['ORIGINAL', 'PRIMARY', 'VOLUME', 'NONE'],
                                                  PixelPresentation='MONOCHROME', VolumetricProperties='VOLUME',
                                                  VolumeBasedCalculationTechnique='NONE')),
        PixelValueTransformationSequence=sequence(item(RescaleIntercept='0', RescaleSlope='1', RescaleType='BQML')),
        FrameVOILUTSequence=sequence(item(WindowCenter='32768', WindowWidth='65536')),
        FrameAnatomySequence=sequence(item(FrameLaterality='U', AnatomicRegionSequence=sequence(
            code('T-D3000', 'SRT', 'Chest')))),
        UnassignedSharedConvertedAttributesSequence=sequence(item(ActualFrameDuration='1000')))
    ds.PerFrameFunctionalGroupsSequence = _per_frame(
        sop.frames, lambda index: dict(
            ConversionSourceAttributesSequence=sequence(item(
                ReferencedSOPClassUID='1.2.840.10008.5.1.4.1.1.128', ReferencedSOPInstanceUID=uids['related'] + f'.{index + 1}')),
            UnassignedPerFrameConvertedAttributesSequence=sequence(item(SliceLocation=str(index)))),
        dimensions=False)
    attach_pixels(ds, sop.pixel_format, sop.frames)


def _segment():
    return item(
        SegmentNumber=1, SegmentLabel='CORPUS', SegmentAlgorithmType='MANUAL',
        SegmentedPropertyCategoryCodeSequence=sequence(code('T-D0050', 'SRT', 'Tissue')),
        SegmentedPropertyTypeCodeSequence=sequence(code('T-D0050', 'SRT', 'Tissue')))


def _segmentation_base(ds, sop, uids):
    common(ds, sop, uids, 'SEG')
    enhanced_equipment(ds)
    frame_of_reference(ds, uids)
    ds.NumberOfFrames = sop.frames
    ds.ImageType = ['DERIVED', 'PRIMARY']
    ds.ContentLabel, ds.ContentDescription = 'CORPUS', 'Corpus segmentation'
    ds.ContentCreatorName = ''
    ds.LossyImageCompression = '00'
    ds.SegmentSequence = sequence(_segment())
    _dimensions(ds, uids, pointer=(0x0062, 0x000B), functional=(0x0062, 0x000A))
    ds.SharedFunctionalGroupsSequence = _shared()
    ds.PerFrameFunctionalGroupsSequence = _per_frame(
        sop.frames, lambda index: dict(SegmentIdentificationSequence=sequence(item(ReferencedSegmentNumber=1))))
    ds.DimensionOrganizationType = '3D'


def build_seg(ds, sop, uids):
    _segmentation_base(ds, sop, uids)
    ds.SegmentationType = 'BINARY'
    attach_pixels(ds, sop.pixel_format, sop.frames)


def build_label_map_seg(ds, sop, uids):
    _segmentation_base(ds, sop, uids)
    ds.SegmentationType = 'LABELMAP'
    attach_pixels(ds, sop.pixel_format, sop.frames)


def build_height_map_seg(ds, sop, uids):
    _segmentation_base(ds, sop, uids)
    ds.SegmentationType = 'HEIGHTMAP'
    attach_pixels(ds, sop.pixel_format, sop.frames)


def build_surface_seg(ds, sop, uids):
    common(ds, sop, uids, 'SEG')
    enhanced_equipment(ds)
    frame_of_reference(ds, uids)
    ds.ContentLabel, ds.ContentDescription = 'CORPUS', 'Corpus surface'
    ds.ContentCreatorName = ''
    ds.ImageType = ['DERIVED', 'PRIMARY']
    ds.SegmentSequence = sequence(_segment())
    ds.SegmentSequence[0].SurfaceCount = 1
    ds.SegmentSequence[0].ReferencedSurfaceSequence = sequence(item(
        ReferencedSurfaceNumber=1, SegmentSurfaceSourceInstanceSequence=sequence(),
        SegmentSurfaceGenerationAlgorithmIdentificationSequence=sequence(
            item(AlgorithmFamilyCodeSequence=sequence(code('123109', 'DCM', 'Manual Processing')),
                 AlgorithmNameCodeSequence=sequence(code('123109', 'DCM', 'Manual Processing')), AlgorithmName='CORPUS', AlgorithmVersion='1'))))
    ds.NumberOfSurfaces = 1
    ds.SurfaceSequence = sequence(item(
        SurfaceNumber=1, SurfaceProcessing='NO', RecommendedDisplayGrayscaleValue=65535,
        RecommendedDisplayCIELabValue=[65535, 32768, 32768], RecommendedPresentationOpacity=1.0,
        RecommendedPresentationType='SURFACE', FiniteVolume='YES', Manifold='YES',
        SurfacePointsSequence=sequence(item(
            NumberOfSurfacePoints=3,
            PointCoordinatesData=np.array([0, 0, 0, 1, 0, 0, 0, 1, 0], dtype='<f4').tobytes())),
        SurfacePointsNormalsSequence=sequence(),
        SurfaceMeshPrimitivesSequence=sequence(item(
            LongTrianglePointIndexList=np.array([1, 2, 3], dtype='<u4').tobytes(),
            LongVertexPointIndexList=b'', LongEdgePointIndexList=b'', TriangleStripSequence=sequence(),
            TriangleFanSequence=sequence(), LineSequence=sequence(), FacetSequence=sequence()))))


def build_parametric_map(ds, sop, uids):
    common(ds, sop, uids, 'RWV')
    enhanced_equipment(ds)
    ds.AcquisitionContextSequence = sequence()
    frame_of_reference(ds, uids)
    ds.NumberOfFrames = sop.frames
    ds.ImageType = ['DERIVED', 'PRIMARY', 'NONE', 'NONE']
    ds.ContentLabel, ds.ContentDescription = 'CORPUS', 'Corpus parametric map'
    ds.ContentCreatorName = ''
    ds.AcquisitionDateTime = DATETIME
    ds.PresentationLUTShape = 'IDENTITY'
    ds.LossyImageCompression = '00'
    ds.BurnedInAnnotation = 'NO'
    ds.RecognizableVisualFeatures = 'NO'
    ds.ContentQualification = 'RESEARCH'
    _dimensions(ds, uids)
    ds.SharedFunctionalGroupsSequence = _shared(
        FrameAnatomySequence=sequence(item(FrameLaterality='U', AnatomicRegionSequence=sequence(
            code('T-D3000', 'SRT', 'Chest')))),
        ParametricMapFrameTypeSequence=sequence(item(FrameType=['DERIVED', 'PRIMARY', 'NONE', 'NONE'])),
        PixelValueTransformationSequence=sequence(item(RescaleIntercept='0', RescaleSlope='1', RescaleType='US')),
        RealWorldValueMappingSequence=sequence(item(
            LUTExplanation='CORPUS', LUTLabel='CORPUS', RealWorldValueFirstValueMapped=0,
            RealWorldValueLastValueMapped=65535, RealWorldValueIntercept=0.0, RealWorldValueSlope=1.0,
            MeasurementUnitsCodeSequence=sequence(code('1', 'UCUM', 'no units')))))
    ds.PerFrameFunctionalGroupsSequence = _per_frame(sop.frames)
    attach_pixels(ds, sop.pixel_format, sop.frames)


def _op_base(ds, sop, uids, modality):
    common(ds, sop, uids, modality)
    ds.ImageType = ['ORIGINAL', 'PRIMARY', '', 'RETINA']
    ds.ImageLaterality = 'R'
    ds.PatientOrientation = ['L', 'F']
    ds.AcquisitionDateTime = DATETIME
    ds.AcquisitionNumber = 1
    ds.BurnedInAnnotation, ds.LossyImageCompression = 'NO', '00'
    ds.PresentationLUTShape = 'IDENTITY'
    ds.ContentDate, ds.ContentTime = DATE, TIME
    ds.HorizontalFieldOfView = 30.0
    ds.AnatomicRegionSequence = sequence(code('T-AA000', 'SRT', 'Eye'))
    ds.PixelSpacing = ['0.1', '0.1']
    ds.DetectorType = 'CCD'
    ds.SynchronizationFrameOfReferenceUID = uids['frame']
    ds.SynchronizationTrigger, ds.AcquisitionTimeSynchronized = 'NO TRIGGER', 'N'
    ds.NumberOfFrames = 1
    ds.FrameTime = '33.3'
    ds.FrameIncrementPointer = (0x0018, 0x1063)
    ds.EmmetropicMagnification, ds.IntraOcularPressure = '', ''


def build_op(ds, sop, uids):
    _op_base(ds, sop, uids, 'OP')
    ds.AcquisitionDeviceTypeCodeSequence = sequence(code('R-1021A', 'SRT', 'Fundus Camera'))
    ds.IlluminationTypeCodeSequence = sequence()
    ds.LightPathFilterTypeStackCodeSequence = sequence()
    ds.ImagePathFilterTypeStackCodeSequence = sequence()
    ds.LensesCodeSequence = sequence()
    ds.PatientEyeMovementCommanded = 'NO'
    ds.PupilDilated = 'NO'
    ds.RefractiveStateSequence = sequence()
    attach_pixels(ds, sop.pixel_format, sop.frames)


def build_wide_field_op(ds, sop, uids):
    _op_base(ds, sop, uids, 'OP')
    ds.AcquisitionDeviceTypeCodeSequence = sequence(code('R-1021A', 'SRT', 'Fundus Camera'))
    ds.IlluminationTypeCodeSequence = sequence()
    ds.LightPathFilterTypeStackCodeSequence = sequence()
    ds.ImagePathFilterTypeStackCodeSequence = sequence()
    ds.LensesCodeSequence = sequence()
    ds.PatientEyeMovementCommanded = 'NO'
    ds.PupilDilated = 'NO'
    ds.RefractiveStateSequence = sequence()
    attach_pixels(ds, sop.pixel_format, sop.frames)


def build_opt(ds, sop, uids):
    common(ds, sop, uids, 'OPT')
    enhanced_equipment(ds)
    ds.AcquisitionContextSequence = sequence()
    # PS3.3 fixes these at 0, 1, 1: the IOD forbids real concatenations.
    ds.ConcatenationFrameOffsetNumber, ds.InConcatenationNumber, ds.InConcatenationTotalNumber = 0, 1, 1
    ds.EmmetropicMagnification, ds.IntraOcularPressure = '', ''
    ds.LightPathFilterTypeStackCodeSequence = sequence()
    frame_of_reference(ds, uids)
    ds.ImageType = ['ORIGINAL', 'PRIMARY', 'TOMOGRAPHIC', 'NONE']
    ds.NumberOfFrames = sop.frames
    ds.ImageLaterality = 'R'
    ds.AcquisitionDateTime = DATETIME
    ds.AcquisitionNumber = 1
    ds.AcquisitionDuration = 1.0
    ds.BurnedInAnnotation, ds.LossyImageCompression = 'NO', '00'
    ds.PresentationLUTShape = 'IDENTITY'
    ds.AcquisitionDeviceTypeCodeSequence = sequence(code('A-00FBE', 'SRT', 'Optical Coherence Tomography Scanner'))
    ds.AnatomicRegionSequence = sequence(code('T-AA000', 'SRT', 'Eye'))
    ds.AxialLengthOfTheEye = 24.0
    ds.HorizontalFieldOfView = 30.0
    ds.RefractiveStateSequence = sequence()
    ds.ContentQualification = 'RESEARCH'
    ds.DepthSpatialResolution, ds.MaximumDepthDistortion = 10.0, 1.0
    ds.AlongScanSpatialResolution, ds.MaximumAlongScanDistortion = 10.0, 1.0
    ds.AcrossScanSpatialResolution, ds.MaximumAcrossScanDistortion = 10.0, 1.0
    ds.DetectorType = 'CCD'
    ds.IlluminationWaveLength, ds.IlluminationPower, ds.IlluminationBandwidth = 840, 1, 50
    ds.PupilDilated = 'NO'
    ds.SharedFunctionalGroupsSequence = _shared(
        FrameAnatomySequence=sequence(item(FrameLaterality='R', AnatomicRegionSequence=sequence(
            code('T-AA000', 'SRT', 'Eye')))),
        OphthalmicFrameLocationSequence=sequence(item(
            ReferencedSOPClassUID='1.2.840.10008.5.1.4.1.1.77.1.5.1', ReferencedSOPInstanceUID=uids['related'],
            ReferenceCoordinates=[0.0, 0.0, 1.0, 1.0], OphthalmicImageOrientation='LINEAR')))
    ds.PerFrameFunctionalGroupsSequence = _per_frame(sop.frames)
    _dimensions(ds, uids)
    attach_pixels(ds, sop.pixel_format, sop.frames)


def build_oct_bscan(ds, sop, uids):
    build_opt(ds, sop, uids)
    ds.ImageType = ['ORIGINAL', 'PRIMARY', 'NONE', 'NONE']
    ds.RecognizableVisualFeatures = 'NO'
    ds.AcquisitionMethodAlgorithmSequence = sequence(item(
        AlgorithmFamilyCodeSequence=sequence(code('123109', 'DCM', 'Manual Processing')),
        AlgorithmNameCodeSequence=sequence(code('123109', 'DCM', 'Manual Processing')),
        AlgorithmName='CORPUS', AlgorithmVersion='1'))
    ds.OCTBscanAnalysisAcquisitionParametersSequence = sequence(item(
        NumberOfBscansPerFrame=1, BscanSlabThickness=1.0, DistanceBetweenBscanSlabs=1.0,
        ASCANRate=1.0, BscanRate=1.0, BscanCycleTime=1.0,
        ScanPatternTypeCodeSequence=sequence(code('128280', 'DCM', 'Volume Raster'))))
    shared = ds.SharedFunctionalGroupsSequence[0]
    shared.FrameVOILUTSequence = sequence(item(WindowCenter='128', WindowWidth='256'))
    for group in ds.PerFrameFunctionalGroupsSequence:
        group.DerivationImageSequence = sequence()


def build_wsi(ds, sop, uids):
    common(ds, sop, uids, 'SM')
    enhanced_equipment(ds)
    ds.NumberOfOpticalPaths = 1
    frame_of_reference(ds, uids)
    ds.NumberOfFrames = sop.frames
    ds.ImageType = ['ORIGINAL', 'PRIMARY', 'VOLUME', 'NONE']
    ds.DimensionOrganizationType = 'TILED_FULL'
    ds.DimensionOrganizationSequence = sequence(item(DimensionOrganizationUID=uids['dimension']))
    ds.AcquisitionDateTime = DATETIME
    ds.AcquisitionDuration = 1.0
    ds.ContentQualification = 'RESEARCH'
    ds.AcquisitionNumber = 1
    ds.BurnedInAnnotation, ds.LossyImageCompression = 'NO', '00'
    ds.SpecimenLabelInImage = 'NO'
    ds.VolumetricProperties = 'VOLUME'
    ds.PixelPresentation = 'TRUE_COLOR'
    ds.ImagedVolumeWidth, ds.ImagedVolumeHeight, ds.ImagedVolumeDepth = 10.0, 10.0, 1.0
    ds.TotalPixelMatrixColumns, ds.TotalPixelMatrixRows = 48, 99
    ds.TotalPixelMatrixFocalPlanes = 1
    ds.TotalPixelMatrixOriginSequence = sequence(item(
        XOffsetInSlideCoordinateSystem=0.0, YOffsetInSlideCoordinateSystem=0.0))
    ds.ImageOrientationSlide = ORIENTATION
    ds.ExtendedDepthOfField, ds.FocusMethod = 'NO', 'AUTO'
    ds.OpticalPathSequence = sequence(item(
        OpticalPathIdentifier='1', IlluminationTypeCodeSequence=sequence(code('111744', 'DCM', 'Brightfield illumination')),
        IlluminationColorCodeSequence=sequence(code('R-102C0', 'SRT', 'Full Spectrum')),
        ICCProfile=b'\0' * 128))
    ds.ContainerIdentifier = 'CORPUS'
    ds.IssuerOfTheContainerIdentifierSequence = sequence()
    ds.ContainerTypeCodeSequence = sequence(code('433466003', 'SCT', 'Microscope slide'))
    ds.SpecimenDescriptionSequence = sequence(item(
        SpecimenIdentifier='CORPUS', SpecimenUID=uids['related'], IssuerOfTheSpecimenIdentifierSequence=sequence(),
        SpecimenPreparationSequence=sequence()))
    ds.AcquisitionContextSequence = sequence()
    ds.SharedFunctionalGroupsSequence = sequence(item(
        PixelMeasuresSequence=sequence(item(PixelSpacing=['0.001', '0.001'], SliceThickness='0.001')),
        WholeSlideMicroscopyImageFrameTypeSequence=sequence(item(FrameType=['ORIGINAL', 'PRIMARY', 'VOLUME', 'NONE'])),
        OpticalPathIdentificationSequence=sequence(item(OpticalPathIdentifier='1'))))
    attach_pixels(ds, sop.pixel_format, sop.frames)


BUILDERS = {
    'ct': build_ct, 'enhanced-ct': build_enhanced_ct, 'legacy-ct': build_legacy_ct, 'mr': build_mr,
    'enhanced-mr': build_enhanced_mr, 'us': build_us, 'us-multiframe': build_us_multiframe, 'xa': build_xa,
    'enhanced-xa': build_enhanced_xa, 'xrf': build_xrf, 'xray3d': build_xray3d, 'dx': build_dx, 'mg': build_mg,
    'io': build_io, 'cr': build_cr, 'sc': build_sc, 'sc-multiframe': build_sc_multiframe, 'nm': build_nm,
    'nm-retired': build_nm_retired, 'pet': build_pet, 'enhanced-pet': build_enhanced_pet, 'legacy-pet': build_legacy_pet,
    'rt-image': build_rt_image, 'rt-dose': build_rt_dose, 'rt-struct': build_rt_struct,
    'rt-beams-record': build_rt_beams_record, 'rt-plan': build_rt_plan, 'rt-brachy-record': build_rt_brachy_record,
    'rt-summary-record': build_rt_summary_record, 'rt-ion-plan': build_rt_ion_plan,
    'rt-ion-record': build_rt_ion_record, 'seg': build_seg, 'surface-seg': build_surface_seg,
    'label-map-seg': build_label_map_seg, 'height-map-seg': build_height_map_seg,
    'parametric-map': build_parametric_map, 'sr-basic': build_sr_basic, 'sr': build_sr, 'sr-cad': build_sr_cad,
    'kos': build_kos, 'sr-dose': build_sr_dose, 'sr-waveform-annotation': build_sr_waveform_annotation,
    'waveform-ps': build_waveform_ps, 'op': build_op, 'opt': build_opt, 'wide-field-op': build_wide_field_op,
    'oct-bscan': build_oct_bscan, 'wsi': build_wsi, 'encapsulated-pdf': build_encapsulated,
    'encapsulated-cda': build_encapsulated,
    **{family: build_waveform for family in WAVEFORMS},
}


def uid_set(root, index):
    base = f'{root}.{index}'
    return dict(study=f'{base}.1', series=f'{base}.2', instance=f'{base}.3', frame=f'{base}.4',
                related=f'{base}.5', dimension=f'{base}.6')


def build_fixture(sop, root, index):
    """Return a native Explicit VR Little Endian dataset for one SOP class."""
    ds = Dataset()
    BUILDERS[sop.family](ds, sop, uid_set(root, index))
    meta = FileMetaDataset()
    meta.MediaStorageSOPClassUID = ds.SOPClassUID
    meta.MediaStorageSOPInstanceUID = ds.SOPInstanceUID
    meta.TransferSyntaxUID = ExplicitVRLittleEndian
    meta.ImplementationClassUID = '2.25.1318'
    ds.file_meta = meta
    ds.is_little_endian, ds.is_implicit_VR = True, False
    ds.preamble = b'\0' * 128
    return ds


def all_fixtures(root):
    for index, sop in enumerate(STORAGE_SOPS, 1):
        yield sop, build_fixture(sop, root, index)
