"""Expected DIMSE-N behavior, transcribed from PS3.4 and run against independent peers and against PACS."""
import base64

MPPS = '1.2.840.10008.3.1.2.3.3'
STORAGE_COMMITMENT, STORAGE_COMMITMENT_INSTANCE = '1.2.840.10008.1.20.1', '1.2.840.10008.1.20.1.1'
UPS_PUSH, UPS_WATCH, UPS_PULL = '1.2.840.10008.5.1.4.34.6.1', '1.2.840.10008.5.1.4.34.6.2', '1.2.840.10008.5.1.4.34.6.3'
UPS_EVENT, UPS_QUERY = '1.2.840.10008.5.1.4.34.6.4', '1.2.840.10008.5.1.4.34.6.5'
UPS_GLOBAL, UPS_FILTERED_GLOBAL = '1.2.840.10008.5.1.4.34.5', '1.2.840.10008.5.1.4.34.5.1'
FILM_SESSION, FILM_BOX = '1.2.840.10008.5.1.1.1', '1.2.840.10008.5.1.1.2'
GRAY_IMAGE_BOX, COLOR_IMAGE_BOX = '1.2.840.10008.5.1.1.4', '1.2.840.10008.5.1.1.4.1'
PRINTER, PRINTER_INSTANCE = '1.2.840.10008.5.1.1.16', '1.2.840.10008.5.1.1.17'
GRAY_META, COLOR_META = '1.2.840.10008.5.1.1.9', '1.2.840.10008.5.1.1.18'
MWL = '1.2.840.10008.5.1.4.31'
SECONDARY_CAPTURE = '1.2.840.10008.5.1.4.1.1.7'

# -- UPS (PS3.4 Annex CC)

UPS_STATES = ('null', 'SCHEDULED', 'IN PROGRESS', 'COMPLETED', 'CANCELED')
SUCCESS = '0000'
# Table CC.1.1-2, event by state: the status, and the state afterwards when it differs from the state before.
# "correct" means the recorded Transaction UID; in SCHEDULED none is recorded, so it means one was offered,
# and "wrong" in SCHEDULED can only mean none was offered. N-CREATE of an existing UID is 0111H everywhere.
UPS_TABLE = {
    'create': dict(zip(UPS_STATES, ((SUCCESS, 'SCHEDULED'), ('0111',), ('0111',), ('0111',), ('0111',)))),
    'progress-correct': dict(zip(UPS_STATES, (('C307',), (SUCCESS, 'IN PROGRESS'), ('C302',), ('C300',), ('C300',)))),
    'progress-wrong': dict(zip(UPS_STATES, (('C307',), ('C301',), ('C301',), ('C301',), ('C301',)))),
    'schedule': dict(zip(UPS_STATES, (('C307',), ('C303',), ('C303',), ('C303',), ('C303',)))),
    'complete-correct': dict(zip(UPS_STATES, (('C307',), ('C310',), (SUCCESS, 'COMPLETED'), ('B306',), ('C300',)))),
    'complete-wrong': dict(zip(UPS_STATES, (('C307',), ('C301',), ('C301',), ('C301',), ('C301',)))),
    'cancel-correct': dict(zip(UPS_STATES, (('C307',), ('C310',), (SUCCESS, 'CANCELED'), ('C300',), ('B304',)))),
    'cancel-wrong': dict(zip(UPS_STATES, (('C307',), ('C301',), ('C301',), ('C301',), ('C301',)))),
    # CC.2.2: IN PROGRESS stays IN PROGRESS, and with no subscriber to tell the request fails as C312H.
    'request-cancel': dict(zip(UPS_STATES, (('C307',), (SUCCESS, 'CANCELED'), ('C312',), ('C311',), ('B304',)))),
}
# Table CC.2.5-3: the P attributes under Unified Procedure Step Performed Procedure Sequence.
PERFORMED_REQUIRED = ('PerformedStationNameCodeSequence', 'PerformedProcedureStepStartDateTime',
                      'PerformedWorkitemCodeSequence', 'PerformedProcedureStepEndDateTime', 'OutputInformationSequence')
CODE_ITEM = dict(CodeValue='110001', CodingSchemeDesignator='DCM', CodeMeaning='Image Processing')


def ups_uid(run, name):
    return f'{run}.{sum(ord(character) for character in name)%9973}.{len(name)}'


def ups_create(label='Validate step', **overrides):
    """A complete N-CREATE of Table CC.2.5-3: Type 1 values, and Type 2 elements present and empty."""
    values = dict(ProcedureStepState='SCHEDULED', ScheduledProcedureStepPriority='MEDIUM', ProcedureStepLabel=label,
                  WorklistLabel='Validation', ScheduledProcedureStepStartDateTime='20261102120000', InputReadinessState='READY',
                  PatientName='UPS^PATIENT', PatientID='UPSID', PatientBirthDate='', PatientSex='',
                  AdmittingDiagnosesDescription='', ReferencedRequestSequence=[], ScheduledStationNameCodeSequence=[],
                  ScheduledWorkitemCodeSequence=[], InputInformationSequence=[], TransactionUID='',
                  UnifiedProcedureStepPerformedProcedureSequence=[], ProcedureStepProgressInformationSequence=[],
                  StudyInstanceUID='1.2.826.0.1.3680043.8.498.1320')
    values.update(overrides)
    return {key: value for key, value in values.items() if value is not None}


def ups_performed(omit=None):
    """Everything COMPLETED needs; `omit` leaves one required attribute out."""
    item = dict(PerformedStationNameCodeSequence=[CODE_ITEM], PerformedProcedureStepStartDateTime='20261102120100',
                PerformedWorkitemCodeSequence=[CODE_ITEM], PerformedProcedureStepEndDateTime='20261102120200',
                OutputInformationSequence=[])
    item.pop(omit, None)
    return dict(UnifiedProcedureStepPerformedProcedureSequence=[item])


def ups_cancellation():
    """Everything CANCELED needs from the SCU that controls the step (code X of the same table)."""
    return dict(ProcedureStepProgressInformationSequence=[
        dict(ProcedureStepCancellationDateTime='20261102120300', ProcedureStepProgress='50')])


def n_create_ups(uid, **overrides):
    return dict(op='n_create', **{'class': UPS_PUSH}, meta=UPS_PUSH, instance=uid, dataset=ups_create(**overrides))


def n_set_ups(uid, transaction, values):
    data = dict(values)
    if transaction is not None:
        data['TransactionUID'] = transaction
    return dict(op='n_set', **{'class': UPS_PUSH}, meta=UPS_PULL, instance=uid, dataset=data)


def change_state(uid, state, transaction):
    data = dict(ProcedureStepState=state)
    if transaction is not None:
        data['TransactionUID'] = transaction
    return dict(op='n_action', **{'class': UPS_PUSH}, meta=UPS_PULL, instance=uid, action=1, dataset=data)


def request_cancel(uid, reason=None):
    return dict(op='n_action', **{'class': UPS_PUSH}, meta=UPS_PUSH, instance=uid, action=2,
                dataset=dict(ReasonForCancellation=reason) if reason else dict(ReasonForCancellation='Not needed'))


def watch_action(instance, action, receiver='WATCHER', lock=None):
    data = dict(ReceivingAE=receiver) if receiver else {}
    if lock is not None:
        data['DeletionLock'] = 'TRUE' if lock else 'FALSE'
    return dict(op='n_action', **{'class': UPS_PUSH}, meta=UPS_WATCH, instance=instance, action=action, dataset=data)


def n_get_ups(uid, tags=()):
    return dict(op='n_get', **{'class': UPS_PUSH}, meta=UPS_PUSH, instance=uid, tags=list(tags))


def ups_setup(uid, state, transaction):
    """Public operations that take one UPS to a state. `null` means nothing is created."""
    steps = []
    if state != 'null':
        steps.append(n_create_ups(uid))
    if state in ('IN PROGRESS', 'COMPLETED', 'CANCELED'):
        steps.append(change_state(uid, 'IN PROGRESS', transaction))
    if state == 'COMPLETED':
        steps += [n_set_ups(uid, transaction, ups_performed()), change_state(uid, 'COMPLETED', transaction)]
    if state == 'CANCELED':
        steps += [n_set_ups(uid, transaction, ups_cancellation()), change_state(uid, 'CANCELED', transaction)]
    return steps


def ups_event_step(event, uid, state, transaction):
    """The request of one table row. Final-state attributes are in place, so C304H cannot hide the row's answer."""
    prepared = []
    if state == 'IN PROGRESS' and event == 'complete-correct':
        prepared.append(n_set_ups(uid, transaction, ups_performed()))
    if state == 'IN PROGRESS' and event == 'cancel-correct':
        prepared.append(n_set_ups(uid, transaction, ups_cancellation()))
    wrong = None if state == 'SCHEDULED' else f'{transaction}.9'
    step = {'create': lambda: n_create_ups(uid),
            'progress-correct': lambda: change_state(uid, 'IN PROGRESS', transaction),
            'progress-wrong': lambda: change_state(uid, 'IN PROGRESS', wrong),
            'schedule': lambda: change_state(uid, 'SCHEDULED', transaction),
            'complete-correct': lambda: change_state(uid, 'COMPLETED', transaction),
            'complete-wrong': lambda: change_state(uid, 'COMPLETED', wrong),
            'cancel-correct': lambda: change_state(uid, 'CANCELED', transaction),
            'cancel-wrong': lambda: change_state(uid, 'CANCELED', wrong),
            'request-cancel': lambda: request_cancel(uid)}[event]()
    return prepared, step


# (name, state before, request builder(uid, transaction), status, state afterwards or None to leave it unchecked)
UPS_SET_CASES = (
    ('scheduled-without-transaction', 'SCHEDULED', lambda u, t: n_set_ups(u, None, dict(ProcedureStepLabel='Renamed')), SUCCESS),
    ('scheduled-with-transaction', 'SCHEDULED', lambda u, t: n_set_ups(u, t, dict(ProcedureStepLabel='Renamed')), 'C310'),
    ('in-progress-correct', 'IN PROGRESS', lambda u, t: n_set_ups(u, t, dict(ProcedureStepLabel='Renamed')), SUCCESS),
    ('in-progress-wrong', 'IN PROGRESS', lambda u, t: n_set_ups(u, f'{t}.9', dict(ProcedureStepLabel='Other')), 'C301'),
    ('in-progress-missing', 'IN PROGRESS', lambda u, t: n_set_ups(u, None, dict(ProcedureStepLabel='Other')), 'C301'),
    ('completed', 'COMPLETED', lambda u, t: n_set_ups(u, t, dict(ProcedureStepLabel='Other')), 'C300'),
    ('canceled', 'CANCELED', lambda u, t: n_set_ups(u, t, dict(ProcedureStepLabel='Other')), 'C300'),
    ('unknown', 'null', lambda u, t: n_set_ups(u, None, dict(ProcedureStepLabel='Other')), 'C307'),
    ('state-by-n-set', 'SCHEDULED', lambda u, t: n_set_ups(u, None, dict(ProcedureStepState='IN PROGRESS')), '0106'),
)
# Allowed by Tables CC.2-1, CC.2-2, CC.2-3 and CC.2-5, as (abstract syntax, operation) pairs; anything else is 0211H.
UPS_OPERATIONS = {UPS_PUSH: {'n_create', 'n_get', 'cancel'}, UPS_PULL: {'n_set', 'n_get', 'state', 'c_find'},
                  UPS_WATCH: {'watch', 'n_get', 'c_find', 'cancel'}, UPS_QUERY: {'n_get', 'c_find'}}
UNRECOGNIZED_OPERATION = '0211'

# -- Storage Commitment (PS3.4 Annex J, failure reasons from PS3.3 C.14.1.1)

NO_SUCH_OBJECT, CLASS_INSTANCE_CONFLICT = 0x0112, 0x0119
MISSING_ATTRIBUTE, NO_SUCH_ACTION, INVALID_INSTANCE = '0120', '0123', '0117'


def commitment_request(transaction, references):
    return dict(TransactionUID=transaction, ReferencedSOPSequence=[
        dict(ReferencedSOPClassUID=sop_class, ReferencedSOPInstanceUID=instance) for sop_class, instance in references])


STGCMT_REFUSALS = (
    ('wrong-action-type', dict(action=2), lambda ref: commitment_request('2.25.1320001', ref), NO_SUCH_ACTION),
    ('missing-transaction-uid', dict(), lambda ref: dict(ReferencedSOPSequence=commitment_request('', ref)['ReferencedSOPSequence']),
     MISSING_ATTRIBUTE),
    ('missing-references', dict(), lambda ref: dict(TransactionUID='2.25.1320002'), MISSING_ATTRIBUTE),
    ('wrong-instance', dict(instance='2.25.13200'), lambda ref: commitment_request('2.25.1320003', ref), INVALID_INSTANCE),
    ('item-without-instance', dict(), lambda ref: dict(TransactionUID='2.25.1320004', ReferencedSOPSequence=[
        dict(ReferencedSOPClassUID=SECONDARY_CAPTURE)]), MISSING_ATTRIBUTE),
)

# -- MPPS (PS3.4 Annex F.7)

MPPS_CREATE = dict(
    PerformedProcedureStepStatus='IN PROGRESS', PerformedProcedureStepID='PPS1', PerformedStationAETitle='MODALITY',
    PerformedStationName='', PerformedLocation='', PerformedProcedureStepStartDate='20261102',
    PerformedProcedureStepStartTime='120000', PerformedProcedureStepEndDate='', PerformedProcedureStepEndTime='',
    PerformedProcedureStepDescription='', PerformedProcedureTypeDescription='', Modality='OT', StudyID='',
    ProcedureCodeSequence=[], PerformedProtocolCodeSequence=[], PerformedSeriesSequence=[], PatientName='MPPS^PATIENT',
    PatientID='MPPSID', PatientBirthDate='', PatientSex='', ReferencedPatientSequence=[],
    ScheduledStepAttributesSequence=[dict(
        StudyInstanceUID='1.2.826.0.1.3680043.8.498.1320.1', ReferencedStudySequence=[], AccessionNumber='MPPSACC',
        RequestedProcedureID='', RequestedProcedureDescription='', ScheduledProcedureStepID='',
        ScheduledProcedureStepDescription='', ScheduledProtocolCodeSequence=[])])


def mpps_completion(series='1.2.826.0.1.3680043.8.498.1320.2', instance='1.2.826.0.1.3680043.8.498.1320.3', status='COMPLETED'):
    return dict(PerformedProcedureStepStatus=status, PerformedProcedureStepEndDate='20261102',
                PerformedProcedureStepEndTime='121000', PerformedSeriesSequence=[dict(
                    SeriesInstanceUID=series, RetrieveAETitle='', SeriesDescription='', PerformingPhysicianName='',
                    OperatorsName='', ProtocolName='validate', ReferencedImageSequence=[dict(
                        ReferencedSOPClassUID=SECONDARY_CAPTURE, ReferencedSOPInstanceUID=instance)],
                    ReferencedNonImageCompositeSOPInstanceSequence=[])])


def mpps_create(uid, **overrides):
    return dict(op='n_create', **{'class': MPPS}, instance=uid, dataset={**MPPS_CREATE, **overrides})


def mpps_set(uid, data):
    return dict(op='n_set', **{'class': MPPS}, instance=uid, dataset=data)


# (name, setup steps, request, status); the reference peer answers them first.
MPPS_REFUSALS = (
    ('duplicate-create', lambda u: [mpps_create(u)], lambda u: mpps_create(u), '0111'),
    ('set-unknown-instance', lambda u: [], lambda u: mpps_set(u, mpps_completion()), '0112'),
    ('set-after-completed', lambda u: [mpps_create(u), mpps_set(u, mpps_completion())],
     lambda u: mpps_set(u, mpps_completion()), '0110'),
    ('set-after-discontinued', lambda u: [mpps_create(u), mpps_set(u, mpps_completion(status='DISCONTINUED'))],
     lambda u: mpps_set(u, mpps_completion()), '0110'),
)

# -- Worklist (PS3.4 Annex K)

MWL_ITEMS = (
    dict(PatientName='ALPHA^ONE', PatientID='MWL1', AccessionNumber='ACC1', StudyInstanceUID='1.2.826.0.1.3680043.8.498.1320.11',
         Modality='CT', ScheduledStationAETitle='CTSCAN', ScheduledProcedureStepStartDate='20261102',
         ScheduledProcedureStepStartTime='090000', ScheduledProcedureStepID='SPS1', ScheduledProcedureStepDescription='Chest',
         RequestedProcedureID='RP1', RequestedProcedureDescription='CT Chest'),
    dict(PatientName='BETA^TWO', PatientID='MWL2', AccessionNumber='ACC2', StudyInstanceUID='1.2.826.0.1.3680043.8.498.1320.12',
         Modality='MR', ScheduledStationAETitle='MRSCAN', ScheduledProcedureStepStartDate='20261103',
         ScheduledProcedureStepStartTime='100000', ScheduledProcedureStepID='SPS2', ScheduledProcedureStepDescription='Head',
         RequestedProcedureID='RP2', RequestedProcedureDescription='MR Head'),
    dict(PatientName='GAMMA^THREE', PatientID='MWL3', AccessionNumber='ACC3', StudyInstanceUID='1.2.826.0.1.3680043.8.498.1320.13',
         Modality='CT', ScheduledStationAETitle='CTSCAN', ScheduledProcedureStepStartDate='20261104',
         ScheduledProcedureStepStartTime='110000', ScheduledProcedureStepID='SPS3', ScheduledProcedureStepDescription='Abdomen',
         RequestedProcedureID='RP3', RequestedProcedureDescription='CT Abdomen'),
)
# (name, findscu -m keys, patient IDs that must come back)
MWL_QUERIES = (
    ('all', (), ('MWL1', 'MWL2', 'MWL3')),
    ('patient-id', ('PatientID=MWL2',), ('MWL2',)),
    ('patient-wildcard', ('PatientName=G*',), ('MWL3',)),
    ('accession', ('AccessionNumber=ACC3',), ('MWL3',)),
    ('modality', ('ScheduledProcedureStepSequence.Modality=CT',), ('MWL1', 'MWL3')),
    ('station', ('ScheduledProcedureStepSequence.ScheduledStationAETitle=MRSCAN',), ('MWL2',)),
    ('date', ('ScheduledProcedureStepSequence.ScheduledProcedureStepStartDate=20261103',), ('MWL2',)),
    ('date-range', ('ScheduledProcedureStepSequence.ScheduledProcedureStepStartDate=20261103-20261104',), ('MWL2', 'MWL3')),
    ('no-match', ('PatientID=NOBODY',), ()),
)

# -- Print (PS3.4 Annex H)

SESSION = dict(NumberOfCopies='1', PrintPriority='MED', MediumType='PAPER', FilmDestination='MAGAZINE')
FILM_BOX_VALUES = dict(ImageDisplayFormat='STANDARD\\1,1', FilmOrientation='PORTRAIT', FilmSizeID='8INX10IN',
                       MagnificationType='REPLICATE', BorderDensity='BLACK', EmptyImageDensity='BLACK')
GRAY_PIXELS = bytes(range(16))


def film_box_values(session_uid, **overrides):
    values = dict(FILM_BOX_VALUES, ReferencedFilmSessionSequence=[
        dict(ReferencedSOPClassUID=FILM_SESSION, ReferencedSOPInstanceUID=session_uid)])
    values.update(overrides)
    return values


def image_box_values(position=1, pixels=GRAY_PIXELS):
    return dict(ImageBoxPosition=position, BasicGrayscaleImageSequence=[dict(
        SamplesPerPixel=1, PhotometricInterpretation='MONOCHROME2', Rows=4, Columns=4, BitsAllocated=8, BitsStored=8,
        HighBit=7, PixelRepresentation=0, PixelAspectRatio=['1', '1'],
        PixelData=base64.b64encode(pixels).decode())])


def print_step(op, sop_class, instance=None, meta=GRAY_META, **fields):
    return dict(op=op, **{'class': sop_class}, instance=instance, meta=meta, **fields)


# -- What each declared SOP Class and role is judged by

def requirement_cases(declared):
    """The named cases behind each declared-support requirement this job owns."""
    print_scp = ['dimse_n.print.scp.dcmprscu', 'dimse_n.print.scp.flow', 'dimse_n.print.scp.printer', 'dimse_n.print.scp.color']
    print_scp_refused = ['dimse_n.print.scp.refuse.*', 'dimse_n.print.scp.lifetime', 'dimse_n.print.scp.names-its-session']
    print_scu = ['dimse_n.print.scu.print', 'dimse_n.print.scu.status', 'dimse_n.print.scu.color']
    print_scu_refused = ['dimse_n.print.scu.refused']
    ups_scp = ['dimse_n.ups.scp.table.*', 'dimse_n.ups.scp.dcm4che.*', 'dimse_n.ups.scp.get.*', 'dimse_n.ups.scp.create.scp-*',
               'dimse_n.ups.scp.persist-*', 'dimse_n.ups.scp.*restart*']
    ups_refused = ['dimse_n.ups.scp.set.*', 'dimse_n.ups.scp.create.*', 'dimse_n.ups.scp.final.*', 'dimse_n.ups.scp.context.*',
                   'dimse_n.ups.scp.concurrent-claim']
    by_sop = {
        MWL: (['dimse_n.mwl.scp.*'], ['dimse_n.mwl.scp.*.refuse.*'], ['dimse_n.mwl.scu.positive'],
              ['dimse_n.mwl.scu.no-results', 'dimse_n.mwl.scu.failure-status']),
        MPPS: (['dimse_n.mpps.scp.*.completes', 'dimse_n.mpps.scp.*.discontinues', 'dimse_n.mpps.scp.*.persist-*'],
               ['dimse_n.mpps.scp.*.refusals'], ['dimse_n.mpps.scu.completes'],
               ['dimse_n.mpps.scu.unknown-instance', 'dimse_n.mpps.scu.duplicate-create']),
        STORAGE_COMMITMENT: (['dimse_n.stgcmt.scp.reverse.*', 'dimse_n.stgcmt.scp.same-association.*', 'dimse_n.stgcmt.scp.released-before-event',
                              'dimse_n.stgcmt.scp.persistence.*', 'dimse_n.stgcmt.scp.retry.late-listener'],
                             ['dimse_n.stgcmt.scp.negative.*', 'dimse_n.stgcmt.scp.retry.unreachable-then-healthy',
                              'dimse_n.stgcmt.scp.retry.failure-reply-not-repeated'],
                             ['dimse_n.stgcmt.scu.reverse', 'dimse_n.stgcmt.scu.same-association'], ['dimse_n.stgcmt.scu.refused']),
        UPS_PUSH: (ups_scp, ups_refused, ['dimse_n.ups.scu.lifecycle', 'dimse_n.ups.scu.request-cancel'], ['dimse_n.ups.scu.failure-statuses']),
        UPS_PULL: (ups_scp, ups_refused, ['dimse_n.ups.scu.lifecycle'], ['dimse_n.ups.scu.failure-statuses']),
        UPS_WATCH: (['dimse_n.ups.scp.watch.*'], ['dimse_n.ups.scp.watch.refusals', 'dimse_n.ups.scp.unreachable-subscriber'],
                    ['dimse_n.ups.scu.subscribe'], ['dimse_n.ups.scu.subscribe']),
        UPS_EVENT: (['dimse_n.ups.scp.watch.*', 'dimse_n.ups.scp.dcm4che.receive', 'dimse_n.ups.scp.retry.*', 'dimse_n.ups.scp.restart-reports-status'],
                    ['dimse_n.ups.scp.unreachable-subscriber'], [], []),
        UPS_QUERY: (['dimse_n.ups.scp.query'], ['dimse_n.ups.scp.context.*'], [], []),
    }
    for uid in (FILM_SESSION, FILM_BOX, GRAY_IMAGE_BOX, COLOR_IMAGE_BOX, PRINTER, GRAY_META, COLOR_META):
        by_sop[uid] = (print_scp, print_scp_refused, print_scu, print_scu_refused)
    mapping = {
        'requirement.normalized_services.positive': sorted({pattern for row in by_sop.values() for pattern in row[0]+row[2]}),
        'requirement.normalized_services.negative': sorted({pattern for row in by_sop.values() for pattern in row[1]+row[3]}),
    }
    for sop in declared['sop_classes']:
        row = by_sop.get(sop['uid'])
        if row is None:
            continue
        for role, (positive, negative) in (('SCP', row[:2]), ('SCU', row[2:])):
            if role in sop['roles']:
                mapping[f'support.sop.{sop["uid"]}.{role}.positive'] = positive
                mapping[f'support.sop.{sop["uid"]}.{role}.negative'] = negative
    return mapping
