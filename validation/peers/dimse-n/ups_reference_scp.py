#!/usr/bin/env python3
"""A UPS SCP written from PS3.4 Annex CC, used to qualify the UPS expectations and peers.

Usage: ups_reference_scp.py --aet AE --port N --record events.jsonl [--peer AE:host:port ...]

No published tool implements a UPS SCP, so this one exists to answer one question before PACS is
involved: do the expected statuses interoperate with an implementation nobody here wrote? dcm4che
upsscu runs against this process, and the same tables then run against PACS. It holds state in memory.

Tables implemented, by name: CC.1.1-2 (state transitions), CC.2.3-2 (subscriptions), CC.2.5-3 (final
state and N-CREATE requirements), CC.2.6-2 (N-SET), CC.2.4-1 (event information).
"""
import argparse
from datetime import datetime
import queue
import threading

from pydicom.dataset import Dataset
from pydicom.uid import generate_uid
from pynetdicom import AE, build_context, build_role, evt

from peer_common import SYNTAXES, append_record, parse_peers, to_plain

UPS_PUSH, UPS_WATCH, UPS_PULL = '1.2.840.10008.5.1.4.34.6.1', '1.2.840.10008.5.1.4.34.6.2', '1.2.840.10008.5.1.4.34.6.3'
UPS_EVENT, UPS_QUERY = '1.2.840.10008.5.1.4.34.6.4', '1.2.840.10008.5.1.4.34.6.5'
GLOBAL, FILTERED_GLOBAL = '1.2.840.10008.5.1.4.34.5', '1.2.840.10008.5.1.4.34.5.1'
SUCCESS, DUPLICATE, MISSING_ATTRIBUTE, INVALID_VALUE, UNRECOGNIZED = 0x0000, 0x0111, 0x0120, 0x0106, 0x0115
ALREADY_CANCELED, ALREADY_COMPLETED = 0xB304, 0xB306
NO_LONGER_UPDATABLE, WRONG_TRANSACTION, ALREADY_IN_PROGRESS, ONLY_BY_CREATE, FINAL_STATE_UNMET = 0xC300, 0xC301, 0xC302, 0xC303, 0xC304
NO_SUCH_UPS, UNKNOWN_RECEIVER, NOT_SCHEDULED, NOT_IN_PROGRESS = 0xC307, 0xC308, 0xC309, 0xC310
ALREADY_COMPLETED_CANCEL, PERFORMER_UNREACHABLE, ACTION_NOT_APPROPRIATE = 0xC311, 0xC312, 0xC314
FINAL_STATES = ('COMPLETED', 'CANCELED')
# Table CC.2.5-3, N-CREATE Type 1 attributes of the Unified Procedure Step instance.
CREATE_REQUIRED = ('ProcedureStepState', 'ScheduledProcedureStepPriority', 'ScheduledProcedureStepStartDateTime',
                   'InputReadinessState', 'ProcedureStepLabel')
# Tables CC.2-1, CC.2-2, CC.2-3 and CC.2-5: what each negotiated SOP Class carries.
OPERATIONS = {UPS_PUSH: {'n_create', 'n_get', 'cancel'}, UPS_PULL: {'n_set', 'n_get', 'state', 'c_find'},
              UPS_WATCH: {'watch', 'n_get', 'c_find', 'cancel'}, UPS_QUERY: {'n_get', 'c_find'}}
UNRECOGNIZED_OPERATION = 0x0211
# Final State code P of the same table, one level under the Performed Procedure Sequence.
COMPLETED_REQUIRED = ('PerformedStationNameCodeSequence', 'PerformedProcedureStepStartDateTime',
                      'PerformedWorkitemCodeSequence', 'PerformedProcedureStepEndDateTime')
ASSIGNMENT_KEYS = ('ScheduledStationNameCodeSequence', 'ScheduledHumanPerformersSequence')


def now():
    return datetime.now().strftime('%Y%m%d%H%M%S')


class Reference:
    def __init__(self, args):
        self.args = args
        self.peers = parse_peers(args.peer)
        self.lock = threading.Lock()
        self.items, self.transactions, self.subscribed, self.global_state = {}, {}, {}, {}
        self.outbox = queue.Queue()
        self.sender = AE(ae_title=args.aet)
        self.sender.acse_timeout = self.sender.dimse_timeout = self.sender.network_timeout = 10
        threading.Thread(target=self.deliver, daemon=True).start()

    # -- events, on associations this process opens with the SCP role (PS3.7 D.3.3.4)

    def deliver(self):
        while True:
            receiver, instance, event_type, information = self.outbox.get()
            host_port = self.peers.get(receiver)
            status = None
            if host_port:
                try:
                    assoc = self.sender.associate(*host_port, contexts=[build_context(UPS_EVENT, list(SYNTAXES))], ae_title=receiver,
                                                  ext_neg=[build_role(UPS_EVENT, scu_role=False, scp_role=True)])
                    if assoc.is_established:
                        reply, _ = assoc.send_n_event_report(information, event_type, UPS_PUSH, instance, meta_uid=UPS_EVENT)
                        status = f'{reply.Status:04X}' if reply else None
                        assoc.release()
                except (ValueError, RuntimeError) as exc:  # One bad report must not silence the ones after it.
                    status = f'error: {exc}'
            append_record(self.args.record, dict(kind='sent', receiver=receiver, instance=instance, event_type=event_type,
                                                 dataset=to_plain(information), status=status))

    def notify(self, receivers, instance, event_type, information):
        for receiver in receivers:
            self.outbox.put((receiver, instance, event_type, information))

    def state_report(self, item):
        report = Dataset()
        report.ProcedureStepState = item.ProcedureStepState
        report.InputReadinessState = item.get('InputReadinessState', 'READY')
        return report

    def report_state(self, uid, receivers=None):
        self.notify(self.subscribed.get(uid, {}) if receivers is None else receivers, uid, 1, self.state_report(self.items[uid]))

    # -- Table CC.1.1-2

    def transition(self, uid, wanted, offered):
        item = self.items[uid]
        state = item.ProcedureStepState
        recorded = self.transactions.get(uid, '')
        if wanted == 'SCHEDULED':
            return ONLY_BY_CREATE
        if not offered or (recorded and offered != recorded):
            return WRONG_TRANSACTION
        if wanted == 'IN PROGRESS':
            if state == 'SCHEDULED':
                self.transactions[uid] = offered
                self.move(uid, wanted)
                return SUCCESS
            return ALREADY_IN_PROGRESS if state == 'IN PROGRESS' else NO_LONGER_UPDATABLE
        if state == 'SCHEDULED':
            return NOT_IN_PROGRESS
        if state in FINAL_STATES:
            if wanted == state:
                return ALREADY_COMPLETED if state == 'COMPLETED' else ALREADY_CANCELED
            return NO_LONGER_UPDATABLE
        if not self.final_state_met(item, wanted):
            return FINAL_STATE_UNMET
        self.move(uid, wanted)
        return SUCCESS

    @staticmethod
    def final_state_met(item, wanted):
        """Table CC.2.5-3: code P for COMPLETED and code X for CANCELED."""
        if wanted == 'COMPLETED':
            performed = item.get('UnifiedProcedureStepPerformedProcedureSequence') or []
            return bool(performed) and all(performed[0].get(key) for key in COMPLETED_REQUIRED) and \
                'OutputInformationSequence' in performed[0]
        progress = item.get('ProcedureStepProgressInformationSequence') or []
        return bool(progress) and bool(progress[0].get('ProcedureStepCancellationDateTime'))

    def move(self, uid, state):
        self.items[uid].ProcedureStepState = state
        self.report_state(uid)

    def request_cancel(self, uid, request, requesting_ae):
        item = self.items[uid]
        state = item.ProcedureStepState
        if state == 'COMPLETED':
            return ALREADY_COMPLETED_CANCEL
        if state == 'CANCELED':
            return ALREADY_CANCELED
        reason = request.get('ReasonForCancellation') if request else None
        receivers = self.subscribed.get(uid, {})
        if state == 'IN PROGRESS':
            if not receivers:
                return PERFORMER_UNREACHABLE
            report = Dataset()
            report.RequestingAE = requesting_ae
            if reason:
                report.ReasonForCancellation = reason
            self.notify(receivers, uid, 2, report)
            return SUCCESS
        # CC.2.2.3: a SCHEDULED step goes through IN PROGRESS to CANCELED, and subscribers see both.
        self.move(uid, 'IN PROGRESS')
        progress = Dataset()
        progress.ProcedureStepCancellationDateTime = now()
        item.ProcedureStepProgressInformationSequence = [progress]
        if reason:
            item.ReasonForCancellation = reason
        self.move(uid, 'CANCELED')
        return SUCCESS

    # -- Table CC.2.3-2

    def subscribe(self, instance, receiver, lock):
        if instance == GLOBAL:
            already = {uid for uid, receivers in self.subscribed.items() if receiver in receivers}
            self.global_state[receiver] = lock
            for uid in self.items:
                if uid not in already:
                    self.subscribed.setdefault(uid, {})[receiver] = lock
                    if lock:
                        self.report_state(uid, [receiver])
            return
        self.subscribed.setdefault(instance, {})[receiver] = lock
        self.report_state(instance, [receiver])

    def unsubscribe(self, instance, receiver):
        if instance == GLOBAL:
            self.global_state.pop(receiver, None)
            for receivers in self.subscribed.values():
                receivers.pop(receiver, None)
        else:
            self.subscribed.get(instance, {}).pop(receiver, None)

    def watch_action(self, action, instance, information):
        receiver = str(information.get('ReceivingAE', '')).strip() if information else ''
        if not receiver:
            return MISSING_ATTRIBUTE
        if action == 3 and instance == FILTERED_GLOBAL:
            return NO_SUCH_UPS  # CC.2.3.3: a filtered global subscription this SCP does not support.
        if action == 5 and instance != GLOBAL:
            return ACTION_NOT_APPROPRIATE
        if instance != GLOBAL and instance not in self.items:
            return NO_SUCH_UPS
        if action == 3:
            if 'DeletionLock' not in information:
                return MISSING_ATTRIBUTE
            if receiver not in self.peers:
                return UNKNOWN_RECEIVER
            self.subscribe(instance, receiver, str(information.DeletionLock).strip() == 'TRUE')
        elif action == 4:
            self.unsubscribe(instance, receiver)
        else:
            self.global_state.pop(receiver, None)
        return SUCCESS

    # -- DIMSE handlers

    @staticmethod
    def allowed(event, operation):
        return operation in OPERATIONS.get(str(event.context.abstract_syntax), ())

    def on_create(self, event):
        uid = event.request.AffectedSOPInstanceUID or generate_uid()
        data = event.attribute_list
        if not self.allowed(event, 'n_create'):
            return UNRECOGNIZED_OPERATION, None
        with self.lock:
            if uid in self.items:
                return DUPLICATE, None
            if data.get('ProcedureStepState') != 'SCHEDULED':
                return NOT_SCHEDULED, None
            if data.get('TransactionUID'):
                return INVALID_VALUE, None
            if any(not data.get(key) for key in CREATE_REQUIRED):
                return MISSING_ATTRIBUTE, None
            if data.ScheduledProcedureStepPriority not in ('HIGH', 'MEDIUM', 'LOW'):
                return INVALID_VALUE, None
            item = Dataset()
            item.update(data)
            item.SOPClassUID, item.SOPInstanceUID = UPS_PUSH, uid
            item.ScheduledProcedureStepModificationDateTime = now()
            if not item.get('WorklistLabel'):
                item.WorklistLabel = 'DEFAULT'  # CC.2.5.1.3: the SCP fills in what the SCU left out.
            if 'TransactionUID' in item:
                del item.TransactionUID
            self.items[uid] = item
            for receiver, lock in self.global_state.items():
                self.subscribed.setdefault(uid, {})[receiver] = lock
                self.report_state(uid, [receiver])
            append_record(self.args.record, dict(kind='created', instance=uid))
            return SUCCESS, item

    def on_set(self, event):
        uid = event.request.RequestedSOPInstanceUID
        changes = event.modification_list
        if not self.allowed(event, 'n_set'):
            return UNRECOGNIZED_OPERATION, None
        with self.lock:
            item = self.items.get(uid)
            if item is None:
                return NO_SUCH_UPS, None
            state = item.ProcedureStepState
            if state in FINAL_STATES:
                return NO_LONGER_UPDATABLE, None
            offered = str(changes.get('TransactionUID', '')).strip()
            if state == 'IN PROGRESS' and offered != self.transactions.get(uid):
                return WRONG_TRANSACTION, None
            if state == 'SCHEDULED' and offered:
                return NOT_IN_PROGRESS, None
            if 'ProcedureStepState' in changes:
                return INVALID_VALUE, None
            assigned = any(key in changes for key in ASSIGNMENT_KEYS)
            progressed = 'ProcedureStepProgressInformationSequence' in changes
            for element in changes:
                if element.keyword != 'TransactionUID':
                    item[element.tag] = element
            if assigned:
                report = Dataset()  # Table CC.2.4-1: the assignment attributes this instance has values for.
                for key in ASSIGNMENT_KEYS:
                    if item.get(key):
                        setattr(report, key, item.get(key))
                self.notify(self.subscribed.get(uid, {}), uid, 5, report)
            if progressed:
                report = Dataset()
                report.ProcedureStepProgressInformationSequence = item.ProcedureStepProgressInformationSequence
                self.notify(self.subscribed.get(uid, {}), uid, 3, report)
            return SUCCESS, None

    def on_get(self, event):
        if not self.allowed(event, 'n_get'):
            return UNRECOGNIZED_OPERATION, None
        with self.lock:
            item = self.items.get(event.request.RequestedSOPInstanceUID)
            if item is None:
                return NO_SUCH_UPS, None
            requested = event.request.AttributeIdentifierList
            reply = Dataset()
            for element in item:
                if not requested or element.tag in requested:
                    reply[element.tag] = element
            return SUCCESS, reply

    def on_action(self, event):
        action, uid = event.action_type, event.request.RequestedSOPInstanceUID
        information = event.action_information
        operation = {1: 'state', 2: 'cancel', 3: 'watch', 4: 'watch', 5: 'watch'}.get(action)
        if operation and not self.allowed(event, operation):
            return UNRECOGNIZED_OPERATION, None
        with self.lock:
            if action in (3, 4, 5):
                return self.watch_action(action, uid, information), None
            if action not in (1, 2):
                return 0x0123, None
            if uid not in self.items:
                return NO_SUCH_UPS, None
            if action == 2:
                return self.request_cancel(uid, information, str(event.assoc.requestor.ae_title)), None
            wanted = str(information.get('ProcedureStepState', '')).strip() if information else ''
            if not wanted:
                return MISSING_ATTRIBUTE, None
            return self.transition(uid, wanted, str(information.get('TransactionUID', '')).strip()), None

    def on_find(self, event):
        keys = event.identifier
        if not self.allowed(event, 'c_find'):
            yield UNRECOGNIZED_OPERATION, None
            return
        for uid, item in list(self.items.items()):
            if all(not element.value or str(item.get(element.tag).value if element.tag in item else '') == str(element.value)
                   for element in keys if element.VR != 'SQ' and element.keyword != 'QueryRetrieveLevel'):
                row = Dataset()
                for element in keys:
                    if element.tag in item:
                        row[element.tag] = item[element.tag]
                    else:
                        row.add_new(element.tag, element.VR, None)
                row.SOPInstanceUID = uid
                yield 0xFF00, row
        yield SUCCESS, None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--aet', required=True)
    parser.add_argument('--port', type=int, required=True)
    parser.add_argument('--record', required=True)
    parser.add_argument('--peer', action='append')
    args = parser.parse_args()
    reference = Reference(args)
    ae = AE(ae_title=args.aet)
    ae.acse_timeout = ae.dimse_timeout = ae.network_timeout = 30
    for uid in (UPS_PUSH, UPS_WATCH, UPS_PULL, UPS_QUERY):
        ae.add_supported_context(uid, list(SYNTAXES))
    print(f'listening on {args.port}', flush=True)
    ae.start_server(('127.0.0.1', args.port), evt_handlers=[
        (evt.EVT_N_CREATE, reference.on_create), (evt.EVT_N_SET, reference.on_set), (evt.EVT_N_GET, reference.on_get),
        (evt.EVT_N_ACTION, reference.on_action), (evt.EVT_C_FIND, reference.on_find)])


if __name__ == '__main__':
    main()
