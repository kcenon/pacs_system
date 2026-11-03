"""Expected DICOMweb behavior, transcribed from PS3.18 (2026d) and run against a reference server and against PACS."""

DICOM_MULTIPART = 'multipart/related; type="application/dicom"'
OCTET_MULTIPART = 'multipart/related; type="application/octet-stream"'
XML_MULTIPART = 'multipart/related; type="application/dicom+xml"'
JSON_MEDIA = 'application/dicom+json'

EXPLICIT, IMPLICIT = '1.2.840.10008.1.2.1', '1.2.840.10008.1.2'
RLE, JPEG_LS, JPEG_BASELINE = '1.2.840.10008.1.2.5', '1.2.840.10008.1.2.4.80', '1.2.840.10008.1.2.4.50'
CT, SECONDARY_CAPTURE, MULTIFRAME_BYTE = '1.2.840.10008.5.1.4.1.1.2', '1.2.840.10008.5.1.4.1.1.7', '1.2.840.10008.5.1.4.1.1.7.2'
BASIC_TEXT_SR = '1.2.840.10008.5.1.4.1.1.88.11'

READ, SEARCH, WRITE = 'dicomweb.read', 'dicomweb.search', 'dicomweb.write'

# Every route the server registers under DICOMweb, with the scopes that open it (statement 8.5). test_dicomweb_cross.py
# holds this list to the CROW_ROUTE lines in the source, so a new route cannot arrive without its authorization row.
# Each row: name, method, path under the root with placeholders, scopes that are enough on their own.
ROUTES = (
    ('study', 'GET', '/studies/{study}', (READ,)),
    ('study-metadata', 'GET', '/studies/{study}/metadata', (READ,)),
    ('series', 'GET', '/studies/{study}/series/{series}', (READ,)),
    ('series-metadata', 'GET', '/studies/{study}/series/{series}/metadata', (READ,)),
    ('instance', 'GET', '/studies/{study}/series/{series}/instances/{instance}', (READ,)),
    ('instance-metadata', 'GET', '/studies/{study}/series/{series}/instances/{instance}/metadata', (READ,)),
    ('frames', 'GET', '/studies/{study}/series/{series}/instances/{instance}/frames/{frames}', (READ,)),
    ('rendered', 'GET', '/studies/{study}/series/{series}/instances/{instance}/rendered', (READ,)),
    ('frame-rendered', 'GET', '/studies/{study}/series/{series}/instances/{instance}/frames/{frames}/rendered', (READ,)),
    ('bulkdata', 'GET', '/studies/{study}/series/{series}/instances/{instance}/bulkdata/{tag}', (READ,)),
    ('store', 'POST', '/studies', (WRITE,)),
    ('store-to-study', 'POST', '/studies/{study}', (WRITE,)),
    ('search-studies', 'GET', '/studies', (READ, SEARCH)),
    ('search-series', 'GET', '/series', (READ, SEARCH)),
    ('search-instances', 'GET', '/instances', (READ, SEARCH)),
    ('search-study-series', 'GET', '/studies/{study}/series', (READ, SEARCH)),
    ('search-study-instances', 'GET', '/studies/{study}/instances', (READ, SEARCH)),
    ('search-series-instances', 'GET', '/studies/{study}/series/{series}/instances', (READ, SEARCH)),
    ('commitment-request', 'POST', '/storage-commitments', (WRITE,)),
    ('commitment-study', 'POST', '/studies/{study}/commitment', (WRITE,)),
    ('commitment-status', 'GET', '/storage-commitments/{transaction}', (READ,)),
    ('commitment-list', 'GET', '/storage-commitments', (READ,)),
    ('wado-uri', 'GET', '@/wado?requestType=WADO&studyUID={study}&seriesUID={series}&objectUID={instance}&contentType=application%2Fdicom', (READ,)),
)
# Answering a CORS preflight tells nothing about any object, so it has no authorization row.
UNAUTHENTICATED_ROUTES = (('OPTIONS', '/dicomweb/<path>'),)

# Tokens that must be refused with 401, whatever the route (RFC 6750 and statement 8.5). "valid" is the base claim set.
REFUSED_TOKENS = ('no-token', 'empty-bearer', 'garbage', 'wrong-signature', 'unpublished-key', 'expired', 'not-yet-valid', 'wrong-issuer',
                  'wrong-audience', 'alg-none', 'hs256-with-public-key', 'basic-scheme')

# PS3.18 10.4.4 and its notes, 8.7.5: name, resource, Accept (None sends no header), status, what a 200 must be.
# A single part type is only for "a single response item" (Note 3), so on a study or series nothing remains: 406.
ACCEPT_CASES = (
    ('study.default-type', 'study', DICOM_MULTIPART, 200, 'multipart-dicom'),
    ('study.unquoted-type', 'study', 'multipart/related; type=application/dicom', 200, 'multipart-dicom'),
    ('study.any', 'study', '*/*', 200, 'multipart-dicom'),
    ('study.weighted', 'study', f'text/plain;q=1.0, {DICOM_MULTIPART};q=0.5', 200, 'multipart-dicom'),
    ('study.syntax-wildcard', 'study', DICOM_MULTIPART+'; transfer-syntax=*', 200, 'multipart-dicom'),
    ('study.syntax-explicit', 'study', f'{DICOM_MULTIPART}; transfer-syntax={EXPLICIT}', 200, 'multipart-dicom'),
    ('study.single-part-only', 'study', 'application/dicom', 406, None),
    ('study.unsupported', 'study', 'text/plain', 406, None),
    ('study.no-accept', 'study', None, 406, None),
    ('study.mixed-families', 'study', 'application/dicom, image/jpeg', 400, None),
    ('series.default-type', 'series', DICOM_MULTIPART, 200, 'multipart-dicom'),
    ('series.any', 'series', '*/*', 200, 'multipart-dicom'),
    ('series.single-part-only', 'series', 'application/dicom', 406, None),
    ('series.unsupported', 'series', 'text/plain', 406, None),
    ('series.no-accept', 'series', None, 406, None),
    ('instance.default-type', 'instance', DICOM_MULTIPART, 200, 'multipart-dicom'),
    ('instance.any', 'instance', '*/*', 200, 'multipart-dicom'),
    ('instance.single-part', 'instance', 'application/dicom', 200, 'single-dicom'),
    ('instance.unsupported', 'instance', 'text/plain', 406, None),
    ('instance.no-accept', 'instance', None, 406, None),
    ('study-metadata.json', 'study-metadata', JSON_MEDIA, 200, 'json'),
    ('study-metadata.any', 'study-metadata', '*/*', 200, 'json'),
    ('study-metadata.unsupported', 'study-metadata', 'image/jpeg', 406, None),
    ('study-metadata.no-accept', 'study-metadata', None, 406, None),
    ('series-metadata.json', 'series-metadata', JSON_MEDIA, 200, 'json'),
    ('series-metadata.unsupported', 'series-metadata', 'image/jpeg', 406, None),
    ('instance-metadata.json', 'instance-metadata', JSON_MEDIA, 200, 'json'),
    ('instance-metadata.unsupported', 'instance-metadata', 'image/jpeg', 406, None),
    ('frames.multipart', 'frames', OCTET_MULTIPART, 200, 'multipart-octet'),
    ('frames.any', 'frames', '*/*', 200, 'multipart-octet'),
    ('frames.single-part-one-frame', 'frames', 'application/octet-stream', 200, 'single-octet'),
    ('frames.single-part-two-frames', 'two-frames', 'application/octet-stream', 406, None),
    ('frames.unsupported', 'frames', 'text/plain', 406, None),
    ('frames.no-accept', 'frames', None, 406, None),
    ('rendered.jpeg', 'rendered', 'image/jpeg', 200, 'jpeg'),
    ('rendered.png', 'rendered', 'image/png', 200, 'png'),
    ('rendered.any', 'rendered', '*/*', 200, 'jpeg'),
    ('rendered.unsupported', 'rendered', 'text/plain', 406, None),
    ('rendered.dicom', 'rendered', 'application/dicom', 406, None),
    ('search.json', 'search', JSON_MEDIA, 200, 'json'),
    ('search.any', 'search', '*/*', 200, 'json'),
    ('search.unsupported', 'search', 'text/plain', 406, None),
    ('search.no-accept', 'search', None, 406, None),
)
# Required by Table 10.4.4-1 and Table 10.6.4-1.
XML_CASES = (
    ('study-metadata.xml', 'study-metadata', XML_MULTIPART, 200, 'multipart-xml'),
    ('series-metadata.xml', 'series-metadata', XML_MULTIPART, 200, 'multipart-xml'),
    ('instance-metadata.xml', 'instance-metadata', XML_MULTIPART, 200, 'multipart-xml'),
    ('search.xml', 'search', XML_MULTIPART, 200, 'multipart-xml'),
)
# Note 2 of Table 10.4.4-1: on an Instance Resource this type asks for the Bulk Data of the resource.
BULK_EQUIVALENT_CASES = (
    ('study.octet-stream', 'study', OCTET_MULTIPART, 200, 'bulk-of-instances'),
    ('instance.octet-stream', 'instance', OCTET_MULTIPART, 200, 'bulk-of-instances'),
)

# PS3.18 10.4.1 "{frames}: a comma-separated list of Frame numbers, in ascending order, contained within an Instance", and
# 200 means "representations for all of the Target Resource(s)": a frame that is not there is not silently left out.
# The standard names no status for a frame past the end: 400 (errors in the parameters) and 404 (no such resource) both refuse.
# Name, frame list, statuses. The frame list comes from the instance with three frames.
BAD_LIST, NO_SUCH_FRAME = (400,), (400, 404)
FRAME_REFUSALS = (
    ('past-the-end', '4', NO_SUCH_FRAME),
    ('far-past-the-end', '99', NO_SUCH_FRAME),
    ('zero', '0', BAD_LIST),
    ('negative', '-1', BAD_LIST),
    ('letters', 'abc', BAD_LIST),
    ('trailing-comma', '1,', BAD_LIST),
    ('double-comma', '1,,2', BAD_LIST),
    ('one-good-one-past', '1,4', NO_SUCH_FRAME),
    ('one-past-one-good', '99,1', NO_SUCH_FRAME),
    ('one-bad-syntax', '1,abc', BAD_LIST),
    ('overflow', '99999999999999999999', BAD_LIST),
    ('descending', '2,1', BAD_LIST),
    ('duplicate', '1,1', BAD_LIST),
)

# PS3.18 10.5.3-1: name, what is posted, status. Each row's objects are built by the probe.
#   valid: stored and listed; invalid: listed under Failed SOP Sequence, never stored.
STOW_REFUSALS = (
    ('text-plain', 415),
    ('single-part-dicom', 415),
    ('multipart-mixed', 415),
    ('no-content-type', 415),
    ('no-boundary', 400),
    ('wrong-boundary', 400),
    ('truncated', 400),
    ('empty-body', 400),
)
# A request whose parts each fail: 409; the failures are named, and nothing is stored. Name of the broken object.
STOW_ALL_FAILED = ('not-dicom', 'no-sop-instance', 'no-sop-class', 'no-study', 'no-series', 'traversal-sop-instance', 'traversal-study',
                   'traversal-series', 'wrong-target-study', 'absolute-path-sop-instance', 'wrong-part-type')
# One of those next to a good object: 202, the good one stored, the bad one named.
STOW_PARTIAL = ('not-dicom', 'no-sop-instance', 'traversal-sop-instance', 'wrong-part-type', 'wrong-target-study')

# Annex I: Failure Reason in Failed SOP Sequence is a required attribute, and 0xA900-style codes apply to mismatched studies.
FAILED_SOP_SEQUENCE, REFERENCED_SOP_SEQUENCE, RETRIEVE_URL, FAILURE_REASON = '00081198', '00081199', '00081190', '00081197'
REFERENCED_CLASS, REFERENCED_INSTANCE = '00081150', '00081155'

# QIDO studies, independent of everything else stored: each key is scoped by PatientID=QID* so other fixtures never match.
QIDO_STUDIES = (
    dict(key='alpha', PatientID='QID1', PatientName='ALPHA^ONE', StudyDate='20260105', StudyTime='101010', AccessionNumber='QA1',
         StudyDescription='CHEST CT', Modality='CT', StudyID='ST1', ReferringPhysicianName='REF^A', PatientBirthDate='19700101', PatientSex='M'),
    dict(key='beta', PatientID='QID2', PatientName='ALPHA^TWO', StudyDate='20260115', StudyTime='111111', AccessionNumber='QA2',
         StudyDescription='HEAD MR', Modality='MR', StudyID='ST2', ReferringPhysicianName='REF^B', PatientBirthDate='19710202', PatientSex='F'),
    dict(key='gamma', PatientID='QID3', PatientName='BETA^THREE', StudyDate='20260215', StudyTime='121212', AccessionNumber='QA3',
         StudyDescription='CHEST CR', Modality='CR', StudyID='ST3', ReferringPhysicianName='REF^C', PatientBirthDate='19720303', PatientSex='O'),
    dict(key='delta', PatientID='QID_4', PatientName='GAMMA%^FOUR', StudyDate='20260301', StudyTime='131313', AccessionNumber='QA%4',
         StudyDescription='ABD CT', Modality='CT', StudyID='ST4', ReferringPhysicianName='REF^D', PatientBirthDate='19730404', PatientSex='M'),
    dict(key='delta-x', PatientID='QIDX4', PatientName='GAMMAX^FOUR', StudyDate='20260302', StudyTime='141414', AccessionNumber='QAX4',
         StudyDescription='ABD CT 2', Modality='CT', StudyID='ST5', ReferringPhysicianName='REF^E', PatientBirthDate='19740505', PatientSex='F'),
    dict(key='case', PatientID='QID5', PatientName='DELTA^LOWER', StudyDate='20260401', StudyTime='151515', AccessionNumber='QA5',
         StudyDescription='Lower Case', Modality='MR', StudyID='ST6', ReferringPhysicianName='REF^F', PatientBirthDate='19750606', PatientSex='M'),
    dict(key='hangul', PatientID='QID6', PatientName='김^철수', StudyDate='20260501', StudyTime='161616', AccessionNumber='QA6',
         StudyDescription='KOREAN', Modality='CT', StudyID='ST7', ReferringPhysicianName='REF^G', PatientBirthDate='19760707', PatientSex='M'),
)
# PS3.18 8.3.4.1 applies the PS3.4 C.2.2 matching rules: `*` and `?` are the only wildcards, so % and _ are ordinary characters.
# Name, parameters, keys that must match, with every row scoped to PatientID=QID* unless the row names PatientID itself.
QIDO_FILTERS = (
    ('patient-id', dict(PatientID='QID1'), ('alpha',)),
    ('patient-id-absent', dict(PatientID='NOSUCH'), ()),
    ('patient-name-star', dict(PatientName='ALPHA*'), ('alpha', 'beta')),
    ('patient-name-question', dict(PatientName='ALPHA^T?O'), ('beta',)),
    ('study-date', dict(StudyDate='20260105'), ('alpha',)),
    ('study-date-range', dict(StudyDate='20260101-20260131'), ('alpha', 'beta')),
    ('study-date-before', dict(StudyDate='-20260131'), ('alpha', 'beta')),
    ('study-date-after', dict(StudyDate='20260401-'), ('case', 'hangul')),
    ('accession', dict(AccessionNumber='QA3'), ('gamma',)),
    ('modalities', dict(ModalitiesInStudy='MR'), ('beta', 'case')),
    ('description', dict(StudyDescription='CHEST*'), ('alpha', 'gamma')),
    ('study-id', dict(StudyID='ST2'), ('beta',)),
    ('literal-underscore-in-id', dict(PatientID='QID_4'), ('delta',)),
    ('literal-percent-in-name', dict(PatientName='GAMMA%^FOUR'), ('delta',)),
    ('literal-percent-in-accession', dict(AccessionNumber='QA%4'), ('delta',)),
    ('tag-key', {'00100020': 'QID1'}, ('alpha',)),
    ('combined', dict(PatientName='ALPHA*', StudyDate='20260101-20260110'), ('alpha',)),
    ('hangul', dict(PatientName='김*'), ('hangul',)),
)
# PS3.18 10.6.3-1: 400 when the query parameter syntax is incorrect. Name, query string.
QIDO_BAD_PARAMETERS = (
    ('limit-beyond-range', 'limit=99999999999999999999999'),
    ('limit-letters', 'limit=abc'),
    ('limit-negative', 'limit=-1'),
    ('offset-letters', 'offset=abc'),
    ('offset-negative', 'offset=-1'),
    ('fuzzy-not-boolean', 'fuzzymatching=maybe'),
    ('not-an-attribute', 'NoSuchAttribute=1'),
    ('repeated-attribute', 'PatientID=A&PatientID=B'),
)
# Page sizes used to walk each search route; the union must be exact, with nothing repeated.
PAGE_SIZES = (1, 7, 50)
PAGE_STUDIES, PAGE_SERIES_INSTANCES = 105, 60

# A key that tries to be SQL is only ever a value that matches nothing.
INJECTION_VALUES = ("' OR '1'='1", "x'; DROP TABLE instances; --", '" OR ""="', '%')

# Where a reference differs from PS3.18 as it stands, with the reason; PACS stays held to the standard. A flagged case that
# passes on the reference fails, so this list cannot go stale. Filled from measurements, each row verified by a real run.
UNSUPPORTED_400 = 'Orthanc answers 400 where 8.5 has 406 for no acceptable media type'
NO_FAILURE_LISTED = 'Orthanc lists no failure for a part it could not read or ignores, and answers 200 or a bare 400'
NO_UID_CHECK = 'Orthanc does not check UID syntax; its files are not named by UIDs, so nothing escapes either'
REFERENCE_DIFFERENCES = {
    '*.wado.instance-single-part': 'application/dicom is Optional in Table 10.4.4-1; Orthanc refuses it',
    '*.frames.single-part': 'application/octet-stream is Optional in Table 10.4.4-1; Orthanc refuses it',
    'negotiation.instance.single-part': 'application/dicom is Optional in Table 10.4.4-1; Orthanc refuses it',
    'negotiation.frames.single-part-one-frame': 'application/octet-stream is Optional in Table 10.4.4-1; Orthanc refuses it',
    'negotiation.*.no-accept': 'Orthanc takes a missing Accept as */*; 8.7.5 says 406',
    'negotiation.*.unsupported': UNSUPPORTED_400,
    'negotiation.*.single-part-only': UNSUPPORTED_400,
    'negotiation.frames.single-part-two-frames': UNSUPPORTED_400,
    'negotiation.rendered.dicom': UNSUPPORTED_400,
    'negotiation.search.unsupported': 'Orthanc ignores Accept on a search',
    'negotiation.search.no-accept': 'Orthanc ignores Accept on a search',
    'negotiation.search.xml': 'Orthanc ignores Accept on a search',
    'negotiation.study.octet-stream': 'Orthanc refuses this type on an Instance Resource; Note 2 of Table 10.4.4-1 makes it the Bulk Data',
    'negotiation.instance.octet-stream': 'Orthanc refuses this type on an Instance Resource; Note 2 of Table 10.4.4-1 makes it the Bulk Data',
    'negative.qido.not-an-attribute': 'Orthanc ignores a key that names no attribute',
    'negative.qido.repeated-attribute': 'Orthanc takes the last of a repeated key; 8.3.4.1 says it shall not be repeated',
    'negative.stow.no-accept': 'Orthanc takes a missing Accept as */*; 8.7.5 says 406',
    'cross.empty-patient-id': 'Orthanc keeps an object with an empty Patient ID; PACS refuses it, over both protocols',
    'qido.warning.fuzzy': 'Orthanc performs fuzzy matching, so it has nothing to warn about',
    'qido.includefield.all': 'Orthanc treats includefield=all as the attributes of its main tags at the instance level',
    'negotiation.accept-query': 'Orthanc ignores the accept Query Parameter on a rendered resource',
    'qido.filter.percent-encoded': 'Orthanc takes a percent-encoded * as a literal character',
    'cross.undeclared-bytes': 'Orthanc reads undeclared bytes in its default encoding, so the name survives; PACS marks them as U+FFFD',
    'negative.rendered.window-width-zero': 'Orthanc renders with a width of 0; PS3.3 C.11.2.1.2 has at least 1 for the linear function',
    '*.rendered.viewport-keeps-proportions': 'Orthanc fills the viewport; 8.3.5.1.3 says the scaling maintains the aspect ratio',
    '*.rendered.window-functions': 'Orthanc does not render linear-exact by the formula of PS3.3 C.11.2.1.3.2; 8.3.5.1.4 names the function',
    'negative.frames.descending': 'Orthanc serves frames in any order; {frames} is defined in ascending order',
    'negative.frames.duplicate': 'Orthanc serves a frame twice when asked; {frames} is defined in ascending order',
    'negative.frames.non-image': 'Orthanc answers 500 for frames of an object with no pixels',
    'negative.stow.no-boundary': 'Orthanc answers 415 for a multipart without a boundary',
    'negative.stow.wrong-boundary': 'Orthanc answers 200 with nothing stored when no part is found',
    'negative.stow.truncated': 'Orthanc answers 200 with nothing stored when no part is found',
    'negative.stow.empty-body': 'Orthanc answers 200 with nothing stored when no part is found',
    'negative.stow.all-failed.not-dicom': NO_FAILURE_LISTED,
    'negative.stow.all-failed.no-sop-instance': NO_FAILURE_LISTED,
    'negative.stow.all-failed.no-sop-class': NO_FAILURE_LISTED,
    'negative.stow.all-failed.wrong-part-type': NO_FAILURE_LISTED,
    'negative.stow.all-failed.traversal-*': NO_UID_CHECK,
    'negative.stow.all-failed.absolute-path-*': NO_UID_CHECK,
    'negative.stow.partial.not-dicom': NO_FAILURE_LISTED,
    'negative.stow.partial.no-sop-instance': NO_FAILURE_LISTED,
    'negative.stow.partial.wrong-part-type': NO_FAILURE_LISTED,
    'negative.stow.partial.traversal-*': NO_UID_CHECK,
    'negative.stow.partial.wrong-target-study': 'Orthanc answers 409 although it stored the good object; 10.5.3-1 has 202',
    'negative.qido.limit-negative': 'Orthanc takes a negative limit',
    'negative.qido.offset-negative': 'Orthanc takes a negative offset',
    'cross.changed-duplicate': 'Orthanc identifies an instance by patient, study, series and SOP UIDs together, so this is a second object to it',
    'qido.filter.limit-with-key': 'Orthanc cannot page a filter on a key outside its main tags',
    'qido.filter.literal-percent-in-*': 'Orthanc normalizes indexed names, dropping characters other than ASCII letters and digits',
    'qido.filter.hangul': 'Orthanc normalizes indexed names, dropping characters other than ASCII letters and digits',
}
# Within a case the reference passes, where it may do otherwise than PACS, with the sentence of PS3.18 PACS follows instead.
REFERENCE_LENIENCY = {
    'lossy-default': 'Orthanc decodes lossy data when no transfer syntax is asked; 8.7.3 keeps data the server only has in lossy form as held',
    'implicit-as-held': 'Orthanc returns Implicit VR for transfer-syntax=*; 8.7.3 says it shall not be used with Web Services',
    'bulk-content-location': 'Orthanc sends no Content-Location on Bulk Data parts; 10.4.3.3.5 says they shall have one',
    'page-warning': 'Orthanc pages exactly but sends no Warning when rows remain; 8.3.4.4 says it shall',
    'bulk-encoded-form': 'Orthanc answers a Bulk Data URI of encapsulated pixels in a form of its own; PACS decodes it or hands over the frames',
    'viewport-resampling': 'Orthanc interpolates where PACS repeats pixels; 8.3.5.1.3 fixes the size, and not how it is reached',
}


def requirement_cases():
    """The named cases behind each requirement row this job owns."""
    return {
        'requirement.web.positive': [
            'web.pacs.stow.*', 'web.pacs.cstore.*', 'web.pacs.negotiation.*', 'web.pacs.qido.filter.*', 'web.pacs.qido.page.*',
            'web.pacs.qido.empty', 'web.pacs.qido.includefield.*', 'web.pacs.qido.warning.*', 'web.pacs.cross.*', 'web.pacs.concurrent.*',
            'web.pacs.large.*', 'web.pacs.restart.*', 'web.pacs.clients.*', 'web.pacs.archive-exact', 'web.pacs.auth.valid.*'],
        'requirement.web.negative': [
            'web.pacs.auth.refused.*', 'web.pacs.auth.scope.*', 'web.pacs.auth.state.*', 'web.pacs.negative.*', 'web.pacs.limits.*', 'web.pacs.exposure.*'],
    }
