"""Enumerate the valid SOP, role, syntax and pixel-format combinations (#1318).

Standard library only, so the gate-tests job can check enumeration without the
validation image. Combinations come from explicit tables with stated reasons,
never from a Cartesian product. Forwarding (bytes pass through unchanged) and
codec support (PACS encodes or decodes) are separate case groups.
"""
from collections import namedtuple
import re

Sop = namedtuple('Sop', 'uid name family pixel_format frames retired')
PixelFormat = namedtuple('PixelFormat', 'key rows columns bits_allocated bits_stored samples photometric signed planar')
Syntax = namedtuple('Syntax', 'uid key lossy tool_family')

# PS3.6 names. Retired classes stay when the registry still advertises them.
STORAGE_SOPS = [
    Sop('1.2.840.10008.5.1.4.1.1.2', 'CT Image Storage', 'ct', 'mono16s', 1, False),
    Sop('1.2.840.10008.5.1.4.1.1.2.1', 'Enhanced CT Image Storage', 'enhanced-ct', 'mono16s', 3, False),
    Sop('1.2.840.10008.5.1.4.1.1.2.2', 'Legacy Converted Enhanced CT Image Storage', 'legacy-ct', 'mono16s', 3, False),
    Sop('1.2.840.10008.5.1.4.1.1.4', 'MR Image Storage', 'mr', 'mono12', 1, False),
    Sop('1.2.840.10008.5.1.4.1.1.4.1', 'Enhanced MR Image Storage', 'enhanced-mr', 'mono16u', 3, False),
    Sop('1.2.840.10008.5.1.4.1.1.6.1', 'Ultrasound Image Storage', 'us', 'rgb8', 1, False),
    Sop('1.2.840.10008.5.1.4.1.1.3.1', 'Ultrasound Multi-frame Image Storage', 'us-multiframe', 'rgb8', 3, False),
    Sop('1.2.840.10008.5.1.4.1.1.6', 'Ultrasound Image Storage (Retired)', 'us', 'mono8', 1, True),
    Sop('1.2.840.10008.5.1.4.1.1.3', 'Ultrasound Multi-frame Image Storage (Retired)', 'us-multiframe', 'mono8', 3, True),
    Sop('1.2.840.10008.5.1.4.1.1.12.1', 'X-Ray Angiographic Image Storage', 'xa', 'mono8', 3, False),
    Sop('1.2.840.10008.5.1.4.1.1.12.1.1', 'Enhanced XA Image Storage', 'enhanced-xa', 'mono12', 3, False),
    Sop('1.2.840.10008.5.1.4.1.1.12.2', 'X-Ray Radiofluoroscopic Image Storage', 'xrf', 'mono8', 3, False),
    Sop('1.2.840.10008.5.1.4.1.1.13.1.1', 'X-Ray 3D Angiographic Image Storage', 'xray3d', 'mono12', 3, False),
    Sop('1.2.840.10008.5.1.4.1.1.13.1.2', 'X-Ray 3D Craniofacial Image Storage', 'xray3d', 'mono12', 3, False),
    Sop('1.2.840.10008.5.1.4.1.1.1.1', 'Digital X-Ray Image Storage - For Presentation', 'dx', 'mono12', 1, False),
    Sop('1.2.840.10008.5.1.4.1.1.1.1.1', 'Digital X-Ray Image Storage - For Processing', 'dx', 'mono12', 1, False),
    Sop('1.2.840.10008.5.1.4.1.1.1.2', 'Digital Mammography X-Ray Image Storage - For Presentation', 'mg', 'mono12', 1, False),
    Sop('1.2.840.10008.5.1.4.1.1.1.2.1', 'Digital Mammography X-Ray Image Storage - For Processing', 'mg', 'mono12', 1, False),
    Sop('1.2.840.10008.5.1.4.1.1.1.3', 'Digital Intra-Oral X-Ray Image Storage - For Presentation', 'io', 'mono8', 1, False),
    Sop('1.2.840.10008.5.1.4.1.1.1.3.1', 'Digital Intra-Oral X-Ray Image Storage - For Processing', 'io', 'mono8', 1, False),
    Sop('1.2.840.10008.5.1.4.1.1.1', 'Computed Radiography Image Storage', 'cr', 'mono12', 1, False),
    Sop('1.2.840.10008.5.1.4.1.1.7', 'Secondary Capture Image Storage', 'sc', 'mono8', 1, False),
    Sop('1.2.840.10008.5.1.4.1.1.7.1', 'Multi-frame Single Bit Secondary Capture Image Storage', 'sc-multiframe', 'mono1', 3, False),
    Sop('1.2.840.10008.5.1.4.1.1.7.2', 'Multi-frame Grayscale Byte Secondary Capture Image Storage', 'sc-multiframe', 'mono8', 3, False),
    Sop('1.2.840.10008.5.1.4.1.1.7.3', 'Multi-frame Grayscale Word Secondary Capture Image Storage', 'sc-multiframe', 'mono16u', 3, False),
    Sop('1.2.840.10008.5.1.4.1.1.7.4', 'Multi-frame True Color Secondary Capture Image Storage', 'sc-multiframe', 'rgb8', 3, False),
    Sop('1.2.840.10008.5.1.4.1.1.20', 'Nuclear Medicine Image Storage', 'nm', 'mono16u', 3, False),
    Sop('1.2.840.10008.5.1.4.1.1.5', 'Nuclear Medicine Image Storage (Retired)', 'nm-retired', 'mono16u', 1, True),
    Sop('1.2.840.10008.5.1.4.1.1.128', 'Positron Emission Tomography Image Storage', 'pet', 'mono16s', 1, False),
    Sop('1.2.840.10008.5.1.4.1.1.130', 'Enhanced PET Image Storage', 'enhanced-pet', 'mono16u', 3, False),
    Sop('1.2.840.10008.5.1.4.1.1.128.1', 'Legacy Converted Enhanced PET Image Storage', 'legacy-pet', 'mono16u', 3, False),
    Sop('1.2.840.10008.5.1.4.1.1.481.1', 'RT Image Storage', 'rt-image', 'mono12', 1, False),
    Sop('1.2.840.10008.5.1.4.1.1.481.2', 'RT Dose Storage', 'rt-dose', 'mono32', 3, False),
    Sop('1.2.840.10008.5.1.4.1.1.481.3', 'RT Structure Set Storage', 'rt-struct', None, 0, False),
    Sop('1.2.840.10008.5.1.4.1.1.481.4', 'RT Beams Treatment Record Storage', 'rt-beams-record', None, 0, False),
    Sop('1.2.840.10008.5.1.4.1.1.481.5', 'RT Plan Storage', 'rt-plan', None, 0, False),
    Sop('1.2.840.10008.5.1.4.1.1.481.6', 'RT Brachy Treatment Record Storage', 'rt-brachy-record', None, 0, False),
    Sop('1.2.840.10008.5.1.4.1.1.481.7', 'RT Treatment Summary Record Storage', 'rt-summary-record', None, 0, False),
    Sop('1.2.840.10008.5.1.4.1.1.481.8', 'RT Ion Plan Storage', 'rt-ion-plan', None, 0, False),
    Sop('1.2.840.10008.5.1.4.1.1.481.9', 'RT Ion Beams Treatment Record Storage', 'rt-ion-record', None, 0, False),
    Sop('1.2.840.10008.5.1.4.1.1.66.4', 'Segmentation Storage', 'seg', 'mono1', 3, False),
    Sop('1.2.840.10008.5.1.4.1.1.66.5', 'Surface Segmentation Storage', 'surface-seg', None, 0, False),
    Sop('1.2.840.10008.5.1.4.1.1.66.7', 'Label Map Segmentation Storage', 'label-map-seg', 'mono8', 3, False),
    Sop('1.2.840.10008.5.1.4.1.1.66.8', 'Height Map Segmentation Storage', 'height-map-seg', 'float32', 3, False),
    Sop('1.2.840.10008.5.1.4.1.1.30', 'Parametric Map Storage', 'parametric-map', 'mono16u', 3, False),
    Sop('1.2.840.10008.5.1.4.1.1.88.11', 'Basic Text SR Storage', 'sr-basic', None, 0, False),
    Sop('1.2.840.10008.5.1.4.1.1.88.22', 'Enhanced SR Storage', 'sr', None, 0, False),
    Sop('1.2.840.10008.5.1.4.1.1.88.33', 'Comprehensive SR Storage', 'sr', None, 0, False),
    Sop('1.2.840.10008.5.1.4.1.1.88.34', 'Comprehensive 3D SR Storage', 'sr', None, 0, False),
    Sop('1.2.840.10008.5.1.4.1.1.88.35', 'Extensible SR Storage', 'sr', None, 0, False),
    Sop('1.2.840.10008.5.1.4.1.1.88.50', 'Mammography CAD SR Storage', 'sr-cad', None, 0, False),
    Sop('1.2.840.10008.5.1.4.1.1.88.59', 'Key Object Selection Document Storage', 'kos', None, 0, False),
    Sop('1.2.840.10008.5.1.4.1.1.88.65', 'Chest CAD SR Storage', 'sr-cad', None, 0, False),
    Sop('1.2.840.10008.5.1.4.1.1.88.67', 'X-Ray Radiation Dose SR Storage', 'sr-dose', None, 0, False),
    Sop('1.2.840.10008.5.1.4.1.1.88.69', 'Colon CAD SR Storage', 'sr-cad', None, 0, False),
    Sop('1.2.840.10008.5.1.4.1.1.88.77', 'Waveform Annotation SR Storage', 'sr-waveform-annotation', None, 0, False),
    Sop('1.2.840.10008.5.1.4.1.1.9.1.1', '12-lead ECG Waveform Storage', 'waveform-ecg12', None, 0, False),
    Sop('1.2.840.10008.5.1.4.1.1.9.1.2', 'General ECG Waveform Storage', 'waveform-ecg', None, 0, False),
    Sop('1.2.840.10008.5.1.4.1.1.9.1.3', 'Ambulatory ECG Waveform Storage', 'waveform-ambulatory', None, 0, False),
    Sop('1.2.840.10008.5.1.4.1.1.9.2.1', 'Hemodynamic Waveform Storage', 'waveform-hemodynamic', None, 0, False),
    Sop('1.2.840.10008.5.1.4.1.1.9.3.1', 'Cardiac Electrophysiology Waveform Storage', 'waveform-ep', None, 0, False),
    Sop('1.2.840.10008.5.1.4.1.1.9.4.1', 'Basic Voice Audio Waveform Storage', 'waveform-voice', None, 0, False),
    Sop('1.2.840.10008.5.1.4.1.1.9.4.2', 'General Audio Waveform Storage', 'waveform-audio', None, 0, False),
    Sop('1.2.840.10008.5.1.4.1.1.9.5.1', 'Arterial Pulse Waveform Storage', 'waveform-arterial', None, 0, False),
    Sop('1.2.840.10008.5.1.4.1.1.9.6.1', 'Respiratory Waveform Storage', 'waveform-respiratory', None, 0, False),
    Sop('1.2.840.10008.5.1.4.1.1.9.6.2', 'Multi-channel Respiratory Waveform Storage', 'waveform-respiratory-multi', None, 0, False),
    Sop('1.2.840.10008.5.1.4.1.1.9.7.1', 'Routine Scalp Electroencephalogram Waveform Storage', 'waveform-eeg', None, 0, False),
    Sop('1.2.840.10008.5.1.4.1.1.9.7.2', 'Electromyogram Waveform Storage', 'waveform-emg', None, 0, False),
    Sop('1.2.840.10008.5.1.4.1.1.9.7.3', 'Electrooculogram Waveform Storage', 'waveform-eog', None, 0, False),
    Sop('1.2.840.10008.5.1.4.1.1.9.7.4', 'Sleep Electroencephalogram Waveform Storage', 'waveform-sleep', None, 0, False),
    Sop('1.2.840.10008.5.1.4.1.1.9.8.1', 'Body Position Waveform Storage', 'waveform-body', None, 0, False),
    Sop('1.2.840.10008.5.1.4.1.1.9.100.1', 'Waveform Presentation State Storage', 'waveform-ps', None, 0, False),
    Sop('1.2.840.10008.5.1.4.1.1.77.1.5.1', 'Ophthalmic Photography 8 Bit Image Storage', 'op', 'mono8', 1, False),
    Sop('1.2.840.10008.5.1.4.1.1.77.1.5.2', 'Ophthalmic Photography 16 Bit Image Storage', 'op', 'mono16u', 1, False),
    Sop('1.2.840.10008.5.1.4.1.1.77.1.5.4', 'Ophthalmic Tomography Image Storage', 'opt', 'mono8', 3, False),
    Sop('1.2.840.10008.5.1.4.1.1.77.1.5.5', 'Wide Field Ophthalmic Photography Stereographic Projection Image Storage', 'wide-field-op', 'rgb8', 1, False),
    Sop('1.2.840.10008.5.1.4.1.1.77.1.5.8', 'Ophthalmic Optical Coherence Tomography B-scan Volume Analysis Storage', 'oct-bscan', 'mono8', 3, False),
    Sop('1.2.840.10008.5.1.4.1.1.77.1.6', 'VL Whole Slide Microscopy Image Storage', 'wsi', 'rgb8', 3, False),
    Sop('1.2.840.10008.5.1.4.1.1.104.1', 'Encapsulated PDF Storage', 'encapsulated-pdf', None, 0, False),
    Sop('1.2.840.10008.5.1.4.1.1.104.2', 'Encapsulated CDA Storage', 'encapsulated-cda', None, 0, False),
]

PIXEL_FORMATS = {format.key: format for format in (
    PixelFormat('mono1', 32, 48, 1, 1, 1, 'MONOCHROME2', False, None),
    PixelFormat('mono8', 33, 48, 8, 8, 1, 'MONOCHROME2', False, None),
    PixelFormat('mono8s', 33, 48, 8, 8, 1, 'MONOCHROME2', True, None),
    PixelFormat('mono8odd', 33, 47, 8, 8, 1, 'MONOCHROME2', False, None),
    PixelFormat('mono12', 33, 48, 16, 12, 1, 'MONOCHROME2', False, None),
    PixelFormat('mono12s', 33, 48, 16, 12, 1, 'MONOCHROME2', True, None),
    PixelFormat('mono16u', 33, 48, 16, 16, 1, 'MONOCHROME2', False, None),
    PixelFormat('mono16s', 33, 48, 16, 16, 1, 'MONOCHROME2', True, None),
    PixelFormat('mono32', 33, 48, 32, 32, 1, 'MONOCHROME2', False, None),
    PixelFormat('float32', 33, 48, 32, 32, 1, 'MONOCHROME2', True, None),
    PixelFormat('rgb8', 33, 48, 8, 8, 3, 'RGB', False, 0),
    PixelFormat('rgb8p1', 33, 48, 8, 8, 3, 'RGB', False, 1),
)}

IMPLICIT = '1.2.840.10008.1.2'
EXPLICIT = '1.2.840.10008.1.2.1'
BIG_ENDIAN = '1.2.840.10008.1.2.2'
DEFLATED = '1.2.840.10008.1.2.1.99'
NEAR_LOSSLESS = '1.2.840.10008.1.2.4.81'

# The syntaxes PACS_SCP negotiates (include/.../detail/scp_transfer_syntaxes.h).
NATIVE_SYNTAXES = [Syntax(IMPLICIT, 'implicit-le', False, None), Syntax(EXPLICIT, 'explicit-le', False, None)]
COMPRESSED_SYNTAXES = [
    Syntax('1.2.840.10008.1.2.4.50', 'jpeg-baseline', True, 'jpeg'),
    Syntax('1.2.840.10008.1.2.4.70', 'jpeg-lossless', False, 'jpeg'),
    Syntax('1.2.840.10008.1.2.4.80', 'jpeg-ls-lossless', False, 'jpeg-ls'),
    Syntax('1.2.840.10008.1.2.4.81', 'jpeg-ls-near', True, 'jpeg-ls'),
    Syntax('1.2.840.10008.1.2.4.90', 'jpeg2000-lossless', False, 'jpeg2000'),
    Syntax('1.2.840.10008.1.2.4.91', 'jpeg2000', True, 'jpeg2000'),
    Syntax('1.2.840.10008.1.2.5', 'rle', False, 'rle'),
    Syntax('1.2.840.10008.1.2.4.201', 'htj2k-lossless', False, 'htj2k'),
    Syntax('1.2.840.10008.1.2.4.202', 'htj2k-rpcl', False, 'htj2k'),
    Syntax('1.2.840.10008.1.2.4.203', 'htj2k', True, 'htj2k'),
]
SYNTAX_BY_UID = {syntax.uid: syntax for syntax in NATIVE_SYNTAXES + COMPRESSED_SYNTAXES}

# Registered but not negotiated; no codec exists. Each must be refused.
UNSUPPORTED_SYNTAXES = {
    BIG_ENDIAN: 'big-endian-network',
    DEFLATED: 'deflated-explicit-le',
    '1.2.840.10008.1.2.4.107': 'hevc-main',
    '1.2.840.10008.1.2.4.108': 'hevc-main10',
    '1.2.840.10008.1.2.4.110': 'jpeg-xl-lossless',
    '1.2.840.10008.1.2.4.111': 'jpeg-xl-recompression',
    '1.2.840.10008.1.2.4.112': 'jpeg-xl',
    '1.2.840.10008.1.2.11': 'frame-deflate',
}

# dciodvfy contradicts itself for these IODs: PS3.3 fixes the concatenation attributes
# at 0, 1 and 1, while its multi-frame table allows them only in a real concatenation.
# Each entry must match the whole reported text; anything else stays an error.
_CONCATENATION_CONFLICT = [
    ('</ConcatenationFrameOffsetNumber(0020,9228)> - Attribute present when condition unsatisfied',
     'PS3.3 C.8.17.7 requires 0; the multi-frame table forbids it outside a concatenation'),
    ('</InConcatenationNumber(0020,9162)> - Attribute present when condition unsatisfied',
     'PS3.3 C.8.17.7 requires 1; the multi-frame table forbids it outside a concatenation'),
    ('</InConcatenationTotalNumber(0020,9163)> - Cannot be less than or equal to one',
     'PS3.3 C.8.17.7 requires 1; the multi-frame table requires more than one'),
]
# dciodvfy also reports these as missing when they are absent, whichever form is used.
_XA_EXPOSURE_CONFLICT = [
    ('</XRayTubeCurrentInmA(0018,9330)> - Attribute present when condition unsatisfied',
     'dciodvfy rejects both presence and absence; PS3.3 C.8.19.3 requires it for ORIGINAL frames'),
    ('</ExposureTimeInms(0018,9328)> - Attribute present when condition unsatisfied',
     'dciodvfy rejects both presence and absence; PS3.3 C.8.19.3 requires it for ORIGINAL frames'),
    ('</ExposureInmAs(0018,9332)> - Attribute present when condition unsatisfied',
     'dciodvfy rejects both presence and absence; PS3.3 C.8.19.3 requires it for ORIGINAL frames'),
]
IOD_VALIDATOR_CONFLICTS = {'opt': _CONCATENATION_CONFLICT, 'oct-bscan': _CONCATENATION_CONFLICT,
                           'enhanced-xa': _XA_EXPOSURE_CONFLICT}
# dciodvfy stops on an assertion for wider pixels, before it reports any attribute.
IOD_VALIDATOR_MAX_BITS = 16


def iod_validator_lines(returncode, output):
    """The report of one dciodvfy run. A run that died said nothing about the object, so it is no verdict."""
    if returncode < 0:
        raise ValueError(f'dciodvfy died on signal {-returncode}: {output[-240:]}')
    return output.splitlines()

# Predeclared before any run: an output outside its bound fails, nothing is tuned.
#   max_abs_error  largest per-sample difference against the source
#   min_psnr_db    smallest full-range PSNR against the source
#   method         required Lossy Image Compression Method (0028,2114)
LOSSY_BOUNDS = {
    '1.2.840.10008.1.2.4.50': dict(min_psnr_db=30.0, method='ISO_10918_1'),
    '1.2.840.10008.1.2.4.81': dict(max_abs_error=5, method='ISO_14495_1'),
    '1.2.840.10008.1.2.4.91': dict(min_psnr_db=30.0, method='ISO_15444_1'),
    '1.2.840.10008.1.2.4.203': dict(min_psnr_db=30.0, method='ISO_15444_15'),
}
# dcm_conv -q: NEAR is (100-q)/10, so 50 selects NEAR 5. JPEG 2000 maps 98 to about 4:1: these
# 33x48 fixtures leave a header-sized budget at the default 12:1, which says little about quality.
LOSSY_QUALITY = {'1.2.840.10008.1.2.4.81': 50, '1.2.840.10008.1.2.4.91': 98}
# Largest per-sample difference between PACS and an independent decoder of one lossy stream.
# Conforming JPEG decoders may round differently; HTJ2K compares separate 9/7 implementations.
# JPEG 2000 compares two callers of one library, so the default of 1 applies.
DECODER_AGREEMENT = {'1.2.840.10008.1.2.4.50': 8, '1.2.840.10008.1.2.4.203': 4}


def _format_valid(format_, syntax):
    """Whether PS3.5 allows this pixel format in this syntax, whoever holds the bytes."""
    if format_.key == 'float32':
        return False, 'Float Pixel Data has no encapsulated representation'
    if format_.bits_allocated == 1:
        return False, 'Single-bit Pixel Data is exercised natively; no codec is claimed for it'
    if format_.bits_allocated == 32:
        return False, 'No codec is claimed for 32-bit Pixel Data'
    if syntax.tool_family == 'jpeg' and syntax.lossy and format_.bits_stored != 8:
        return False, 'JPEG Baseline Process 1 is 8-bit only'
    if syntax.tool_family == 'jpeg' and syntax.lossy and format_.signed:
        return False, 'JPEG Baseline codes unsigned samples, so lossy errors would wrap signed patterns'
    if syntax.uid == NEAR_LOSSLESS and format_.signed:
        return False, 'JPEG-LS NEAR bounds bit patterns, so a signed sample can wrap; DCMTK and PACS both refuse it'
    return True, ''


def wire_valid(sop, syntax):
    """Forwarding never decodes, so only the object's own format limits it."""
    if syntax.tool_family is None:
        return True, ''
    if sop.pixel_format is None:
        return False, 'The IOD has no Pixel Data to encapsulate'
    return _format_valid(PIXEL_FORMATS[sop.pixel_format], syntax)


def codec_valid(format_key, syntax):
    """Whether PACS claims to encode and decode this format itself."""
    format_ = PIXEL_FORMATS[format_key]
    ok, reason = _format_valid(format_, syntax)
    if ok and syntax.tool_family == 'jpeg' and not syntax.lossy and format_.samples != 1:
        return False, 'The built-in JPEG Lossless codec claims grayscale only; see the explicit rejection case'
    return ok, reason


def wire_cases():
    """One case per valid SOP, syntax and transfer role."""
    cases, exclusions = [], []
    for sop in STORAGE_SOPS:
        for syntax in NATIVE_SYNTAXES + COMPRESSED_SYNTAXES:
            ok, reason = wire_valid(sop, syntax)
            if not ok:
                exclusions.append(dict(sop=sop.uid, syntax=syntax.uid, reason=reason))
                continue
            for role in ('scp', 'scu', 'forward'):
                cases.append(dict(case_id=f'corpus.wire.{role}.{sop.uid}.{syntax.key}', group='wire', role=role,
                                  sop=sop.uid, syntax=syntax.uid, format=sop.pixel_format, frames=sop.frames))
    return cases, exclusions


# Formats exercised by PACS codecs; frame counts separate single and multi-frame.
CODEC_FORMATS = ['mono8', 'mono8s', 'mono8odd', 'mono12', 'mono12s', 'mono16u', 'mono16s', 'rgb8', 'rgb8p1']
CODEC_FRAMES = [1, 3]


def codec_cases():
    """PACS encode and decode directions, kept apart from wire forwarding."""
    cases = []
    for key in CODEC_FORMATS:
        for syntax in COMPRESSED_SYNTAXES:
            if not codec_valid(key, syntax)[0]:
                continue
            for frames in CODEC_FRAMES:
                variant = f'{key}.f{frames}'
                cases.append(dict(case_id=f'corpus.codec.encode.{syntax.key}.{variant}', group='codec',
                                  direction='encode', syntax=syntax.uid, format=key, frames=frames))
                cases.append(dict(case_id=f'corpus.codec.decode.{syntax.key}.{variant}', group='codec',
                                  direction='decode', syntax=syntax.uid, format=key, frames=frames))
    return cases


def requirement_matches(pattern, case_ids):
    """Cases a requirement pattern names. A UID stays whole: ...1.1.7 does not cover ...1.1.7.1."""
    regex = re.compile(re.escape(pattern).replace(r'\*', '.*')+r'(\.[a-z][^.]*)*$')
    return [case for case in case_ids if regex.match(case)]


def requirement_cases(declared):
    """Attach the declared-support requirements to the corpus case that evaluates them."""
    storage = {sop.uid for sop in STORAGE_SOPS}
    mapping = {}
    for requirement in declared['cases']:
        case_id = requirement['case_id']
        if requirement.get('owner_issue') != 1318:
            continue
        if requirement['sop_class_uids']:
            uid = requirement['sop_class_uids'][0]
            if uid not in storage:
                continue
            role = requirement['roles'][0].lower()
            prefix = ('corpus.negative' if requirement['condition'] == 'negative' else 'corpus.wire')
            mapping[case_id] = f'{prefix}.{role}.{uid}'
        elif requirement['syntaxes']:
            uid = requirement['syntaxes'][0]
            boundary = case_id.rsplit('.', 1)[1]
            if uid == BIG_ENDIAN and boundary != 'network':
                mapping[case_id] = 'file.big-endian'
            elif uid in UNSUPPORTED_SYNTAXES:
                mapping[case_id] = (f'corpus.reject.network.{UNSUPPORTED_SYNTAXES[uid]}' if boundary == 'network'
                                    else f'corpus.reject.codec.{UNSUPPORTED_SYNTAXES[uid]}')
            elif uid in SYNTAX_BY_UID:
                key = SYNTAX_BY_UID[uid].key
                mapping[case_id] = (f'corpus.wire.scp.*.{key}' if boundary == 'network'
                                    else f'corpus.codec.*.{key}' if SYNTAX_BY_UID[uid].tool_family
                                    else f'corpus.file.native.{key}')
    return mapping
