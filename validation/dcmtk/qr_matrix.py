"""Query/Retrieve fixture and case tables (#1319).

Standard library only, so the gate-tests job can check the tables without the
validation image. Every expected result is a literal set of fixture keys, never
the output of a matcher, so a wrong matcher cannot agree with itself. Nonmatching
entities are part of the fixture so that an SCP ignoring a key returns too much.
"""
from collections import namedtuple

CT = '1.2.840.10008.5.1.4.1.1.2'
MR = '1.2.840.10008.5.1.4.1.1.4'
CR = '1.2.840.10008.5.1.4.1.1.1'
SC = '1.2.840.10008.5.1.4.1.1.7'
MODELS = {
    'patient-root': dict(flag='-P', find='1.2.840.10008.5.1.4.1.2.1.1', move='1.2.840.10008.5.1.4.1.2.1.2',
                         get='1.2.840.10008.5.1.4.1.2.1.3', levels=('PATIENT', 'STUDY', 'SERIES', 'IMAGE')),
    'study-root': dict(flag='-S', find='1.2.840.10008.5.1.4.1.2.2.1', move='1.2.840.10008.5.1.4.1.2.2.2',
                       get='1.2.840.10008.5.1.4.1.2.2.3', levels=('STUDY', 'SERIES', 'IMAGE')),
    'patient-study-only': dict(flag='-O', find='1.2.840.10008.5.1.4.1.2.3.1', levels=('PATIENT', 'STUDY')),
}
UID_ROOT = '1.2.3.1319'

Patient = namedtuple('Patient', 'key id name birth sex charset')
Study = namedtuple('Study', 'key patient date time accession description study_id referring')
Series = namedtuple('Series', 'key study modality number description')
Instance = namedtuple('Instance', 'key series sop_class number')

PATIENTS = [
    Patient('P1', 'QR-P1', 'DOE^JOHN', '19700101', 'M', 'ISO_IR 6'),
    Patient('P2', 'QR-P2', 'DOE^JANE', '19800202', 'F', 'ISO_IR 6'),
    Patient('P3', 'QR-P3', 'SMITH^ANNA', '19900303', 'F', 'ISO_IR 6'),
    Patient('P4', 'QR-P4', '홍^길동', '20000404', 'M', 'ISO_IR 192'),
    Patient('P5', 'QR-P5', 'MÜLLER^HANS', '19600505', 'M', 'ISO_IR 100'),
    Patient('P6', 'QR-P6', "O'BRIEN^SEAN", '19500606', 'M', 'ISO_IR 6'),
    Patient('P7', 'QR_P7', '100%^SALE', '19400707', 'F', 'ISO_IR 6'),
    Patient('P8', 'QRXP7', '100XX^SALE', '19400708', 'F', 'ISO_IR 6'),
    Patient('P9', 'QR-P9', '山田^太郎', '19300909', 'M', r'\ISO 2022 IR 87'),
    Patient('P10', 'QR-P10', '김^철수', '19201010', 'F', r'\ISO 2022 IR 149'),
]
STUDIES = [
    Study('ST1', 'P1', '20240115', '101500', 'ACC-001', 'CHEST CT', 'S1', 'HOUSE^GREG'),
    Study('ST2', 'P1', '20240220', '143000', 'ACC-002', 'BRAIN MR', 'S2', 'WILSON^JAMES'),
    Study('ST3', 'P2', '20230310', '090000', 'ACC-003', 'CHEST CT', 'S3', 'HOUSE^GREG'),
    Study('ST4', 'P3', '20240115', '120000', 'ACC-004', 'HAND XR', 'S4', ''),
    Study('ST5', 'P4', '20250505', '080000', 'ACC-005', '한글 검사', 'S5', ''),
    Study('ST6', 'P5', '20220101', '235959', 'ACC-006', 'Thorax Übersicht', 'S6', ''),
    Study('ST7', 'P6', '20211231', '000000', "ACC'7", "O'BRIEN STUDY", 'S7', ''),
    Study('ST8', 'P7', '20200229', '110000', 'ACC_8', '100% CHECK', 'S8', ''),
    Study('ST9', 'P8', '20200301', '110000', 'ACCX8', '100X CHECK', 'S9', ''),
    Study('ST10', 'P9', '20190101', '100000', 'ACC-010', '胸部', 'S10', ''),
    Study('ST11', 'P10', '20180101', '100000', 'ACC-011', '흉부', 'S11', ''),
    # Accession Number is a Required Key with no stored value: any supplied value matches (C.2.2.1.2).
    Study('ST12', 'P3', '20210601', '200000', '', 'EMPTY ACCESSION', 'S12', ''),
]
SERIES = [
    Series('SE1a', 'ST1', 'CT', 1, 'AXIAL'),
    Series('SE1b', 'ST1', 'CT', 2, 'CORONAL'),
    Series('SE1c', 'ST1', 'OT', 3, 'SCOUT'),
    Series('SE2a', 'ST2', 'MR', 1, 'T1'),
    Series('SE3a', 'ST3', 'CT', 1, 'AXIAL'),
    Series('SE4a', 'ST4', 'CR', 1, 'PA'),
    Series('SE5a', 'ST5', 'CT', 1, '축상'),
    Series('SE6a', 'ST6', 'CT', 1, 'Übersicht'),
    Series('SE7a', 'ST7', 'CT', 1, 'PLAIN'),
    Series('SE8a', 'ST8', 'CT', 1, 'PLAIN'),
    Series('SE9a', 'ST9', 'CT', 1, 'PLAIN'),
    Series('SE10a', 'ST10', 'CT', 1, 'PLAIN'),
    Series('SE11a', 'ST11', 'CT', 1, 'PLAIN'),
    Series('SE12a', 'ST12', 'XA', 1, 'PLAIN'),
]
INSTANCES = [
    Instance('I1a1', 'SE1a', CT, 1),
    Instance('I1a2', 'SE1a', CT, 2),
    Instance('I1b1', 'SE1b', CT, 1),
    Instance('I1c1', 'SE1c', SC, 1),
    Instance('I2a1', 'SE2a', MR, 1),
    Instance('I2a2', 'SE2a', MR, 2),
    Instance('I3a1', 'SE3a', CT, 1),
    Instance('I4a1', 'SE4a', CR, 1),
    Instance('I5a1', 'SE5a', CT, 1),
    Instance('I6a1', 'SE6a', CT, 1),
    Instance('I7a1', 'SE7a', CT, 1),
    Instance('I8a1', 'SE8a', CT, 1),
    Instance('I9a1', 'SE9a', CT, 1),
    Instance('I10a1', 'SE10a', CT, 1),
    Instance('I11a1', 'SE11a', CT, 1),
    Instance('I12a1', 'SE12a', CT, 1),
]
PATIENT_BY_KEY = {row.key: row for row in PATIENTS}
STUDY_BY_KEY = {row.key: row for row in STUDIES}
SERIES_BY_KEY = {row.key: row for row in SERIES}
INSTANCE_BY_KEY = {row.key: row for row in INSTANCES}


def study_uid(key):
    return f'{UID_ROOT}.{STUDIES.index(STUDY_BY_KEY[key])+1}'


def series_uid(key):
    row = SERIES_BY_KEY[key]
    siblings = [item.key for item in SERIES if item.study == row.study]
    return f'{study_uid(row.study)}.{siblings.index(key)+1}'


def instance_uid(key):
    row = INSTANCE_BY_KEY[key]
    siblings = [item.key for item in INSTANCES if item.series == row.series]
    return f'{series_uid(row.series)}.{siblings.index(key)+1}'


def modalities_in_study(key):
    """Sorted so the value is stable; DICOM defines no order for this multi-valued key."""
    return sorted({row.modality for row in SERIES if row.study == key})


# The key that identifies an entity in a response at each level.
IDENTITY = {'PATIENT': 'PatientID', 'STUDY': 'StudyInstanceUID', 'SERIES': 'SeriesInstanceUID',
            'IMAGE': 'SOPInstanceUID'}
# Return keys requested at each level, so response values can be compared to the fixture.
RETURN_KEYS = {
    'PATIENT': ('PatientID', 'PatientName', 'PatientBirthDate', 'PatientSex'),
    'STUDY': ('StudyInstanceUID', 'StudyDate', 'StudyTime', 'AccessionNumber', 'StudyID', 'StudyDescription',
              'ReferringPhysicianName', 'ModalitiesInStudy'),
    'SERIES': ('SeriesInstanceUID', 'Modality', 'SeriesNumber', 'SeriesDescription'),
    'IMAGE': ('SOPInstanceUID', 'SOPClassUID', 'InstanceNumber'),
}
# Attributes C.4.1.1.3.2 lets a response carry beyond the request.
RESPONSE_EXTRAS = {'QueryRetrieveLevel', 'SpecificCharacterSet', 'RetrieveAETitle', 'InstanceAvailability'}
# Keys the index does not hold. C.2.2.1.3: the value is ignored, the attribute is not
# returned, and Table C.4-1 reports it with Pending status FF01.
UNSUPPORTED_KEYS = {'PatientComments', 'StudyComments', 'BodyPartExamined', 'ImageComments'}

FindCase = namedtuple('FindCase', 'name model level keys expected')
UTF8 = {'SpecificCharacterSet': 'ISO_IR 192'}
ALL_PATIENTS = [row.key for row in PATIENTS]
ALL_STUDIES = [row.key for row in STUDIES]


def uids(function, *keys):
    return '\\'.join(function(key) for key in keys)


def _patient_cases(model):
    table = [
        ('universal', {}, ALL_PATIENTS),
        ('id-exact', {'PatientID': 'QR-P1'}, ['P1']),
        ('id-case-sensitive', {'PatientID': 'qr-p1'}, []),
        ('id-single-char-wildcard', {'PatientID': 'QR-P?'}, ['P1', 'P2', 'P3', 'P4', 'P5', 'P6', 'P9']),
        ('id-star-wildcard', {'PatientID': 'QR-P1*'}, ['P1', 'P10']),
        ('id-underscore-is-literal', {'PatientID': 'QR_P7'}, ['P7']),
        ('id-quote-injection', {'PatientID': "' OR '1'='1"}, []),
        ('name-exact', {'PatientName': 'DOE^JOHN'}, ['P1']),
        ('name-prefix', {'PatientName': 'DOE*'}, ['P1', 'P2']),
        ('name-single-char', {'PatientName': 'DOE^JO?N'}, ['P1']),
        ('name-ignores-case', {'PatientName': 'doe^john'}, ['P1']),
        ('name-percent-is-literal', {'PatientName': '100%^SALE'}, ['P7']),
        ('name-nomatch', {'PatientName': 'NOBODY*'}, []),
        ('name-quote', {'PatientName': "O'BRIEN*"}, ['P6']),
        ('name-quote-injection', {'PatientName': "' OR '1'='1"}, []),
        ('name-drop-table', {'PatientName': "x'; DROP TABLE patients; --"}, []),
        ('birth-exact', {'PatientBirthDate': '19800202'}, ['P2']),
        ('birth-range', {'PatientBirthDate': '19600101-19801231'}, ['P1', 'P2', 'P5']),
        ('birth-open-start', {'PatientBirthDate': '-19301231'}, ['P9', 'P10']),
        ('birth-open-end', {'PatientBirthDate': '20000101-'}, ['P4']),
        ('birth-wildcard-is-literal', {'PatientBirthDate': '1980*'}, []),
        ('sex', {'PatientSex': 'F'}, ['P2', 'P3', 'P7', 'P8', 'P10']),
        ('keys-are-anded', {'PatientName': 'DOE*', 'PatientSex': 'F'}, ['P2']),
        ('utf8-exact', {**UTF8, 'PatientName': '홍^길동'}, ['P4']),
        ('utf8-wildcard', {**UTF8, 'PatientName': '홍*'}, ['P4']),
        ('utf8-finds-latin1-object', {**UTF8, 'PatientName': 'MÜLLER*'}, ['P5']),
        ('utf8-finds-iso2022-japanese-object', {**UTF8, 'PatientName': '山田*'}, ['P9']),
        ('utf8-finds-iso2022-korean-object', {**UTF8, 'PatientName': '김*'}, ['P10']),
        ('latin1-key', {'SpecificCharacterSet': 'ISO_IR 100', 'PatientName': 'MÜLLER^HANS'}, ['P5']),
        ('iso2022-korean-key', {'SpecificCharacterSet': r'\ISO 2022 IR 149', 'PatientName': '김^철수'}, ['P10']),
        ('iso2022-japanese-key', {'SpecificCharacterSet': r'\ISO 2022 IR 87', 'PatientName': '山田^太郎'}, ['P9']),
        ('utf8-nomatch', {**UTF8, 'PatientName': '없는*'}, []),
        # C.2.2.2.4: "?" is one character, whatever its encoded length.
        ('utf8-question-mark-is-one-character', {**UTF8, 'PatientName': '홍^길?'}, ['P4']),
        ('utf8-question-mark-is-not-one-byte', {**UTF8, 'PatientName': '홍^길???'}, []),
        ('unsupported-key-is-ignored', {'PatientComments': 'NO SUCH TEXT'}, ALL_PATIENTS),
    ]
    return [FindCase(name, model, 'PATIENT', keys, expected) for name, keys, expected in table]


def _study_root_study_cases():
    table = [
        ('universal', {}, ALL_STUDIES),
        ('uid-single', {'StudyInstanceUID': study_uid('ST2')}, ['ST2']),
        ('uid-list', {'StudyInstanceUID': uids(study_uid, 'ST1', 'ST4')}, ['ST1', 'ST4']),
        ('uid-wildcard-is-literal', {'StudyInstanceUID': UID_ROOT+'.*'}, []),
        ('uid-quote-injection', {'StudyInstanceUID': "1' OR '1'='1"}, []),
        ('patient-id', {'PatientID': 'QR-P1'}, ['ST1', 'ST2']),
        ('patient-name', {'PatientName': 'DOE*'}, ['ST1', 'ST2', 'ST3']),
        ('patient-name-injection', {'PatientName': "x' OR '1'='1"}, []),
        ('date-exact', {'StudyDate': '20240115'}, ['ST1', 'ST4']),
        ('date-range', {'StudyDate': '20240101-20240228'}, ['ST1', 'ST2', 'ST4']),
        ('date-open-start', {'StudyDate': '-20191231'}, ['ST10', 'ST11']),
        ('date-open-end', {'StudyDate': '20240301-'}, ['ST5']),
        ('date-leap-day', {'StudyDate': '20200229'}, ['ST8']),
        ('date-wildcard-is-literal', {'StudyDate': '2024*'}, []),
        ('date-quote-injection', {'StudyDate': "20240115' OR '1'='1"}, []),
        ('time-range', {'StudyTime': '100000-103000'}, ['ST1', 'ST10', 'ST11']),
        ('accession-exact', {'AccessionNumber': 'ACC-002'}, ['ST2', 'ST12']),
        ('accession-wildcard', {'AccessionNumber': 'ACC-00?'}, ['ST1', 'ST2', 'ST3', 'ST4', 'ST5', 'ST6', 'ST12']),
        ('accession-underscore-is-literal', {'AccessionNumber': 'ACC_8'}, ['ST8', 'ST12']),
        ('accession-quote', {'AccessionNumber': "ACC'7"}, ['ST7', 'ST12']),
        ('description-suffix', {'StudyDescription': '*CT'}, ['ST1', 'ST3']),
        ('description-percent-is-literal', {'StudyDescription': '100%*'}, ['ST8']),
        ('description-utf8', {**UTF8, 'StudyDescription': '한글*'}, ['ST5']),
        ('description-finds-latin1-object', {**UTF8, 'StudyDescription': '*Übersicht'}, ['ST6']),
        ('description-finds-iso2022-object', {**UTF8, 'StudyDescription': '胸*'}, ['ST10']),
        # C.2.2.1.3: a supported Optional Key behaves as a Required one, so no stored value matches too.
        ('referring-physician', {'ReferringPhysicianName': 'HOUSE*'},
         ['ST1', 'ST3', 'ST4', 'ST5', 'ST6', 'ST7', 'ST8', 'ST9', 'ST10', 'ST11', 'ST12']),
        ('study-id-is-exact', {'StudyID': 'S1'}, ['ST1']),
        ('modality-single', {'ModalitiesInStudy': 'MR'}, ['ST2']),
        ('modality-any-of-list', {'ModalitiesInStudy': r'CR\MR'}, ['ST2', 'ST4']),
        ('modality-any-member-of-study', {'ModalitiesInStudy': 'OT'}, ['ST1']),
        ('keys-are-anded', {'PatientName': 'DOE*', 'StudyDate': '20240101-'}, ['ST1', 'ST2']),
        ('nomatch', {'StudyDescription': 'NOPE'}, []),
        ('unsupported-key-is-ignored', {'StudyComments': 'NO SUCH TEXT', 'StudyDate': '20240115'}, ['ST1', 'ST4']),
    ]
    return [FindCase(name, 'study-root', 'STUDY', keys, expected) for name, keys, expected in table]


def _patient_root_study_cases(model):
    table = [
        ('under-patient', {'PatientID': 'QR-P1'}, ['ST1', 'ST2']),
        ('date-under-patient', {'PatientID': 'QR-P1', 'StudyDate': '20240115'}, ['ST1']),
        ('date-of-other-patient', {'PatientID': 'QR-P2', 'StudyDate': '20240115'}, []),
        ('unknown-patient', {'PatientID': 'NOPE'}, []),
        ('patient-key-is-literal', {'PatientID': 'QR-P*'}, []),
    ]
    return [FindCase(name, model, 'STUDY', keys, expected) for name, keys, expected in table]


def _series_cases():
    one = {'StudyInstanceUID': study_uid('ST1')}
    study = [
        ('all-of-study', dict(one), ['SE1a', 'SE1b', 'SE1c']),
        ('modality', {**one, 'Modality': 'CT'}, ['SE1a', 'SE1b']),
        ('modality-wildcard', {**one, 'Modality': 'C*'}, ['SE1a', 'SE1b']),
        ('number', {**one, 'SeriesNumber': '2'}, ['SE1b']),
        ('description-suffix', {**one, 'SeriesDescription': '*XIAL'}, ['SE1a']),
        ('series-uid', {**one, 'SeriesInstanceUID': series_uid('SE1b')}, ['SE1b']),
        ('series-uid-list', {**one, 'SeriesInstanceUID': uids(series_uid, 'SE1a', 'SE1c')}, ['SE1a', 'SE1c']),
        ('series-of-other-study', {'StudyInstanceUID': study_uid('ST2'), 'SeriesInstanceUID': series_uid('SE1a')}, []),
        ('unknown-study', {'StudyInstanceUID': UID_ROOT+'.999'}, []),
        ('unsupported-key-is-ignored', {**one, 'BodyPartExamined': 'NO SUCH TEXT'}, ['SE1a', 'SE1b', 'SE1c']),
        ('description-utf8', {**UTF8, 'StudyInstanceUID': study_uid('ST5'), 'SeriesDescription': '축*'}, ['SE5a']),
        ('description-finds-latin1-object',
         {**UTF8, 'StudyInstanceUID': study_uid('ST6'), 'SeriesDescription': 'Über*'}, ['SE6a']),
    ]
    patient = [
        ('under-patient', {'PatientID': 'QR-P1', **one}, ['SE1a', 'SE1b', 'SE1c']),
        ('modality-under-patient', {'PatientID': 'QR-P1', **one, 'Modality': 'OT'}, ['SE1c']),
        ('study-of-other-patient', {'PatientID': 'QR-P2', **one}, []),
    ]
    return ([FindCase(name, 'study-root', 'SERIES', keys, expected) for name, keys, expected in study]
            + [FindCase(name, 'patient-root', 'SERIES', keys, expected) for name, keys, expected in patient])


def _image_cases():
    one = {'StudyInstanceUID': study_uid('ST1'), 'SeriesInstanceUID': series_uid('SE1a')}
    study = [
        ('all-of-series', dict(one), ['I1a1', 'I1a2']),
        ('number', {**one, 'InstanceNumber': '2'}, ['I1a2']),
        ('sop-uid', {**one, 'SOPInstanceUID': instance_uid('I1a2')}, ['I1a2']),
        ('sop-uid-list', {**one, 'SOPInstanceUID': uids(instance_uid, 'I1a1', 'I1a2')}, ['I1a1', 'I1a2']),
        ('sop-uid-of-other-series', {**one, 'SOPInstanceUID': instance_uid('I1b1')}, []),
        ('sop-class', {**one, 'SOPClassUID': CT}, ['I1a1', 'I1a2']),
        ('sop-class-other', {**one, 'SOPClassUID': MR}, []),
        ('series-of-other-study', {'StudyInstanceUID': study_uid('ST2'), 'SeriesInstanceUID': series_uid('SE1a')}, []),
        ('unsupported-key-is-ignored', {**one, 'ImageComments': 'NO SUCH TEXT'}, ['I1a1', 'I1a2']),
    ]
    patient = [
        ('under-patient', {'PatientID': 'QR-P1', **one}, ['I1a1', 'I1a2']),
        ('series-of-other-patient', {'PatientID': 'QR-P2', **one}, []),
    ]
    return ([FindCase(name, 'study-root', 'IMAGE', keys, expected) for name, keys, expected in study]
            + [FindCase(name, 'patient-root', 'IMAGE', keys, expected) for name, keys, expected in patient])


def find_cases():
    return (_patient_cases('patient-root') + _patient_cases('patient-study-only')
            + _study_root_study_cases()
            + _patient_root_study_cases('patient-root') + _patient_root_study_cases('patient-study-only')
            + _series_cases() + _image_cases())


# Requests an SCP must answer with status A900: the Identifier does not fit the SOP Class.
# C.4.1.2.1 obliges an SCU to send a single Unique Key value for every level above the
# query level, so a request without one is invalid rather than a search with no matches.
Refusal = namedtuple('Refusal', 'name model level keys')
_ONE_STUDY = {'StudyInstanceUID': study_uid('ST1')}
_ONE_SERIES = {**_ONE_STUDY, 'SeriesInstanceUID': series_uid('SE1a')}
_TWO_STUDIES = {'StudyInstanceUID': uids(study_uid, 'ST1', 'ST2')}
FIND_REFUSALS = [
    Refusal('study-root-has-no-patient-level', 'study-root', 'PATIENT', {'PatientID': ''}),
    Refusal('patient-study-only-has-no-series-level', 'patient-study-only', 'SERIES', {'PatientID': 'QR-P1'}),
    Refusal('patient-study-only-has-no-image-level', 'patient-study-only', 'IMAGE', {'PatientID': 'QR-P1'}),
    Refusal('unknown-level', 'study-root', 'BOGUS', {'PatientID': ''}),
    Refusal('empty-level', 'study-root', '', {'PatientID': ''}),
    Refusal('missing-level', 'patient-root', None, {'PatientID': ''}),
    Refusal('patient-root-study-without-patient', 'patient-root', 'STUDY', {}),
    Refusal('patient-root-study-without-patient-filtered', 'patient-root', 'STUDY', {'StudyDate': '20240115'}),
    Refusal('patient-study-only-study-without-patient', 'patient-study-only', 'STUDY', {}),
    Refusal('study-root-series-without-study', 'study-root', 'SERIES', {'Modality': 'CT'}),
    Refusal('study-root-series-under-two-studies', 'study-root', 'SERIES', dict(_TWO_STUDIES)),
    Refusal('patient-root-series-without-patient', 'patient-root', 'SERIES', dict(_ONE_STUDY)),
    Refusal('patient-root-series-without-study', 'patient-root', 'SERIES', {'PatientID': 'QR-P1'}),
    Refusal('study-root-image-without-series', 'study-root', 'IMAGE', dict(_ONE_STUDY)),
    Refusal('study-root-image-without-study', 'study-root', 'IMAGE', {'SeriesInstanceUID': series_uid('SE1a')}),
    Refusal('study-root-image-under-two-series', 'study-root', 'IMAGE',
            {**_ONE_STUDY, 'SeriesInstanceUID': uids(series_uid, 'SE1a', 'SE1b')}),
    Refusal('patient-root-image-without-patient', 'patient-root', 'IMAGE', dict(_ONE_SERIES)),
]

RetrieveCase = namedtuple('RetrieveCase', 'name model level keys expected')
_ST1 = ['I1a1', 'I1a2', 'I1b1', 'I1c1']
_ST2 = ['I2a1', 'I2a2']
_PATIENT_ONE = {'PatientID': 'QR-P1'}
RETRIEVE_CASES = [
    RetrieveCase('study', 'study-root', 'STUDY', dict(_ONE_STUDY), _ST1),
    # Required Keys do not belong in the Identifier, but they cannot widen what the Unique Key selects.
    RetrieveCase('study-ignores-required-key', 'study-root', 'STUDY', {**_ONE_STUDY, 'StudyDate': '19000101'}, _ST1),
    RetrieveCase('study-list', 'study-root', 'STUDY', {'StudyInstanceUID': uids(study_uid, 'ST2', 'ST4')},
                 [*_ST2, 'I4a1']),
    RetrieveCase('study-unknown', 'study-root', 'STUDY', {'StudyInstanceUID': UID_ROOT+'.999'}, []),
    RetrieveCase('series', 'study-root', 'SERIES', dict(_ONE_SERIES), ['I1a1', 'I1a2']),
    RetrieveCase('series-list', 'study-root', 'SERIES',
                 {**_ONE_STUDY, 'SeriesInstanceUID': uids(series_uid, 'SE1a', 'SE1b')}, ['I1a1', 'I1a2', 'I1b1']),
    RetrieveCase('series-of-other-study', 'study-root', 'SERIES',
                 {'StudyInstanceUID': study_uid('ST2'), 'SeriesInstanceUID': series_uid('SE1a')}, []),
    RetrieveCase('image', 'study-root', 'IMAGE', {**_ONE_SERIES, 'SOPInstanceUID': instance_uid('I1a2')}, ['I1a2']),
    RetrieveCase('image-list', 'study-root', 'IMAGE',
                 {**_ONE_SERIES, 'SOPInstanceUID': uids(instance_uid, 'I1a1', 'I1a2')}, ['I1a1', 'I1a2']),
    RetrieveCase('image-of-other-series', 'study-root', 'IMAGE',
                 {**_ONE_SERIES, 'SOPInstanceUID': instance_uid('I1b1')}, []),
    RetrieveCase('patient', 'patient-root', 'PATIENT', dict(_PATIENT_ONE), [*_ST1, *_ST2]),
    RetrieveCase('patient-unknown', 'patient-root', 'PATIENT', {'PatientID': 'NOPE'}, []),
    RetrieveCase('study', 'patient-root', 'STUDY', {**_PATIENT_ONE, **_ONE_STUDY}, _ST1),
    RetrieveCase('study-of-other-patient', 'patient-root', 'STUDY', {'PatientID': 'QR-P2', **_ONE_STUDY}, []),
    RetrieveCase('series', 'patient-root', 'SERIES', {**_PATIENT_ONE, **_ONE_SERIES}, ['I1a1', 'I1a2']),
    RetrieveCase('image', 'patient-root', 'IMAGE',
                 {**_PATIENT_ONE, **_ONE_SERIES, 'SOPInstanceUID': instance_uid('I1a1')}, ['I1a1']),
]
RETRIEVE_REFUSALS = [
    Refusal('study-root-has-no-patient-level', 'study-root', 'PATIENT', dict(_PATIENT_ONE)),
    Refusal('unknown-level', 'study-root', 'BOGUS', dict(_ONE_STUDY)),
    Refusal('missing-level', 'study-root', None, dict(_ONE_STUDY)),
    Refusal('study-without-study-uid', 'study-root', 'STUDY', {'PatientID': 'QR-P1'}),
    Refusal('series-without-series-uid', 'study-root', 'SERIES', dict(_ONE_STUDY)),
    Refusal('series-without-study-uid', 'study-root', 'SERIES', {'SeriesInstanceUID': series_uid('SE1a')}),
    Refusal('image-without-sop-uid', 'study-root', 'IMAGE', dict(_ONE_SERIES)),
    Refusal('image-without-series-uid', 'study-root', 'IMAGE', {**_ONE_STUDY, 'SOPInstanceUID': instance_uid('I1a1')}),
    Refusal('patient-without-id', 'patient-root', 'PATIENT', {'StudyInstanceUID': study_uid('ST1')}),
    Refusal('patient-id-list', 'patient-root', 'PATIENT', {'PatientID': 'QR-P1\\QR-P2'}),
    Refusal('patient-root-study-without-patient', 'patient-root', 'STUDY', dict(_ONE_STUDY)),
    # A lower Unique Key at a higher level is ambiguous, and ignoring it would deliver more than was asked.
    Refusal('study-level-with-series-key', 'study-root', 'STUDY', dict(_ONE_SERIES)),
]


def _differences():
    """Where dcmqrscp 3.6.9 differs from these expectations, and why PACS still expects them."""
    implementation_defined = 'PN case handling is implementation dependent (C.2.2.2.1.1); PACS declares ASCII case-insensitive'
    bytewise = 'dcmqrscp matches bytes without decoding, so "?" is one byte; C.2.2.2.4 says one character'
    its_optional_key = 'dcmqrscp holds and matches Patient Comments; the PACS index does not hold it'
    empty_required = 'dcmqrscp does not apply C.2.2.1.2: a zero length stored value matches any supplied value'
    no_such_key = 'dcmqrscp does not index this Optional Key and ignores it'
    upper_wildcard = 'dcmqrscp applies wild cards to the Unique Key above the level; C.4.1.2.1 asks for a single value'
    upper_list = 'dcmqrscp accepts several Unique Key values above the level; C.4.1.2.1 asks for a single value'
    filters = 'dcmqrscp filters on a Required Key in a retrieve Identifier; PACS ignores it (C.2.2.1.2: it does not belong there)'
    lenient = 'dcmqrscp does not enforce the Unique Keys of C.2.2.1.1 for retrieves and sends what the other keys select'
    table = {}
    for model in ('patient-root', 'patient-study-only'):
        table[f'find.{model}.patient.name-ignores-case'] = implementation_defined
        table[f'find.{model}.patient.utf8-question-mark-is-one-character'] = bytewise
        table[f'find.{model}.patient.utf8-question-mark-is-not-one-byte'] = bytewise
        table[f'find.{model}.patient.unsupported-key-is-ignored'] = its_optional_key
        table[f'find.{model}.study.patient-key-is-literal'] = upper_wildcard
    for name in ('accession-exact', 'accession-wildcard', 'accession-underscore-is-literal', 'accession-quote',
                 'referring-physician'):
        table[f'find.study-root.study.{name}'] = empty_required
    for name in ('modality-single', 'modality-any-of-list', 'modality-any-member-of-study'):
        table[f'find.study-root.study.{name}'] = no_such_key
    table['find.study-root.series.description-suffix'] = no_such_key
    table['find.study-root.image.sop-class-other'] = no_such_key
    table['find.refuse.study-root-series-under-two-studies'] = upper_list
    table['find.refuse.study-root-image-under-two-series'] = upper_list
    for operation in ('get', 'move'):
        table[f'{operation}.study-root.study.study-ignores-required-key'] = filters
        for name in ('study-root-has-no-patient-level', 'study-without-study-uid', 'series-without-series-uid',
                     'series-without-study-uid', 'image-without-sop-uid', 'image-without-series-uid', 'patient-without-id',
                     'patient-id-list', 'patient-root-study-without-patient', 'study-level-with-series-key'):
            table[f'{operation}.refuse.{name}'] = lenient
    return table


# A listed case that agrees with the expectation fails as stale, so this stays exact.
REFERENCE_DIFFERENCES = _differences()


FIND_UIDS = {model: spec['find'] for model, spec in MODELS.items()}


def requirement_cases(declared):
    """The named cases behind each declared-support requirement this job owns."""
    mapping = {
        'requirement.association.positive': ['qr.verification.scp.*', 'qr.verification.scu.positive'],
        'requirement.association.negative': ['qr.verification.*refused*', 'qr.protocol.find-scp.*'],
        'requirement.query.positive': ['qr.find.*.study-root.*', 'qr.find.*.patient-root.*'],
        'requirement.query.negative': ['qr.find.*.refuse.*', 'qr.fault.database-failure.find', 'qr.cancel.find',
                                       'qr.protocol.find-scp.cancel-*'],
    }
    verification = '1.2.840.10008.1.1'
    for sop in declared['sop_classes']:
        uid, roles = sop['uid'], sop['roles']
        if uid == verification:
            mapping[f'support.sop.{uid}.SCP.positive'] = ['qr.verification.scp.*']
            mapping[f'support.sop.{uid}.SCP.negative'] = ['qr.verification.scp-refused*']
            mapping[f'support.sop.{uid}.SCU.positive'] = ['qr.verification.scu.positive']
            mapping[f'support.sop.{uid}.SCU.negative'] = ['qr.verification.scu-refused*']
        for model, spec in MODELS.items():
            if uid == spec.get('find'):
                if 'SCP' in roles:
                    mapping[f'support.sop.{uid}.SCP.positive'] = [f'qr.find.*.{model}.*']
                    mapping[f'support.sop.{uid}.SCP.negative'] = [f'qr.find.*.refuse.{model}-*']
                if 'SCU' in roles:
                    mapping[f'support.sop.{uid}.SCU.positive'] = [f'qr.scu.find_scu.{model}.*', f'qr.scu.query_scu.{model}.*']
                    mapping[f'support.sop.{uid}.SCU.negative'] = ['qr.scu.fail.find_scu*', 'qr.scu.fail.query_scu*']
            for operation, tools in (('move', ('move_scu.move', 'retrieve_scu.move')), ('get', ('get_scu.get', 'retrieve_scu.get'))):
                if uid == spec.get(operation):
                    if 'SCP' in roles:
                        mapping[f'support.sop.{uid}.SCP.positive'] = [f'qr.{operation}.*.{model}.*']
                        mapping[f'support.sop.{uid}.SCP.negative'] = [f'qr.{operation}.*.refuse.*', f'qr.fault.*{operation}*']
                    if 'SCU' in roles:
                        mapping[f'support.sop.{uid}.SCU.positive'] = [f'qr.scu.{tool}.{model}.*' for tool in tools]
                        mapping[f'support.sop.{uid}.SCU.negative'] = [f'qr.scu.fail.{tools[0].split(".")[0]}*',
                                                                      f'qr.scu.fail.{tools[1].split(".")[0]}*']
    return mapping


def case_id(prefix, row):
    return f'{prefix}.{row.model}.{(row.level or "none").lower()}.{row.name}'
