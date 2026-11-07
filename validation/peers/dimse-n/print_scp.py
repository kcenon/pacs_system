#!/usr/bin/env python3
"""A Print SCP written from PS3.4 Annex H, which records what it is asked to print.

Usage: print_scp.py --aet AE --port N --record jobs.jsonl

dcmprscp only accepts the Basic Grayscale Print Meta SOP Class, so nothing independent can receive a
color print. This peer can, and DCMTK dcmprscu runs against it first to show it understands a real client.
"""
import argparse
import hashlib

from pydicom.dataset import Dataset
from pydicom.sequence import Sequence
from pydicom.uid import generate_uid
from pynetdicom import AE, evt
from pynetdicom.dimse_primitives import N_CREATE

from peer_common import SYNTAXES, append_record

FILM_SESSION, FILM_BOX = '1.2.840.10008.5.1.1.1', '1.2.840.10008.5.1.1.2'
GRAY_BOX, COLOR_BOX = '1.2.840.10008.5.1.1.4', '1.2.840.10008.5.1.1.4.1'
PRINTER, GRAY_META, COLOR_META = '1.2.840.10008.5.1.1.16', '1.2.840.10008.5.1.1.9', '1.2.840.10008.5.1.1.18'
IMAGE_PIXEL = ('SamplesPerPixel', 'PhotometricInterpretation', 'Rows', 'Columns', 'BitsAllocated', 'BitsStored', 'HighBit',
               'PixelRepresentation', 'PixelData')
MISSING_ATTRIBUTE, INVALID_VALUE, NO_SUCH_INSTANCE, EMPTY_PAGE = 0x0120, 0x0106, 0x0112, 0xB603


class Printer:
    def __init__(self, record):
        self.record = record
        self.sessions, self.boxes, self.images = {}, {}, {}
        self.assigned = None

    def on_accepted(self, event):
        """pynetdicom copies the response's UID from the request, before a handler can choose one."""
        dimse = event.assoc.dimse
        send = dimse.send_msg

        def send_with_uid(primitive, context_id):
            if isinstance(primitive, N_CREATE) and not primitive.AffectedSOPInstanceUID:
                primitive.AffectedSOPInstanceUID = self.assigned
            send(primitive, context_id)
        dimse.send_msg = send_with_uid

    def on_create(self, event):
        sop_class = event.request.AffectedSOPClassUID
        uid = event.request.AffectedSOPInstanceUID or generate_uid()
        data = event.attribute_list
        self.assigned = uid
        reply = Dataset()
        if sop_class == FILM_SESSION:
            self.sessions[uid] = dict(copies=str(data.get('NumberOfCopies', '1')), medium=str(data.get('MediumType', '')))
            return 0x0000, data
        if not data.get('ImageDisplayFormat') or not data.get('ReferencedFilmSessionSequence'):
            return MISSING_ATTRIBUTE, None
        if data.ReferencedFilmSessionSequence[0].ReferencedSOPInstanceUID not in self.sessions:
            return INVALID_VALUE, None
        image_class = COLOR_BOX if event.context.abstract_syntax == COLOR_META else GRAY_BOX
        references = []
        columns, rows = str(data.ImageDisplayFormat).split('\\')[1].split(',')
        for _ in range(int(columns)*int(rows)):
            box = Dataset()
            box.ReferencedSOPClassUID, box.ReferencedSOPInstanceUID = image_class, generate_uid()
            references.append(box)
            self.images[box.ReferencedSOPInstanceUID] = uid
        self.boxes[uid] = dict(printed=[])
        reply.update(data)
        reply.ReferencedImageBoxSequence = Sequence(references)
        return 0x0000, reply

    def on_set(self, event):
        sop_class, uid = event.request.RequestedSOPClassUID, event.request.RequestedSOPInstanceUID
        data = event.modification_list
        if sop_class not in (GRAY_BOX, COLOR_BOX):
            return 0x0000, None
        if uid not in self.images:
            return NO_SUCH_INSTANCE, None
        keyword = 'BasicColorImageSequence' if sop_class == COLOR_BOX else 'BasicGrayscaleImageSequence'
        sequence = data.get(keyword)
        if not sequence or 'ImageBoxPosition' not in data:
            return MISSING_ATTRIBUTE, None
        image = sequence[0]
        if any(key not in image for key in IMAGE_PIXEL):
            return MISSING_ATTRIBUTE, None
        self.boxes[self.images[uid]]['printed'].append(dict(
            image_class=sop_class, position=int(data.ImageBoxPosition), photometric=str(image.PhotometricInterpretation),
            rows=int(image.Rows), columns=int(image.Columns), sha256=hashlib.sha256(bytes(image.PixelData)).hexdigest()))
        return 0x0000, None

    def on_action(self, event):
        uid = event.request.RequestedSOPInstanceUID
        if event.action_type != 1:
            return 0x0123, None
        box = self.boxes.get(uid)
        if box is None:
            return NO_SUCH_INSTANCE, None
        if not box['printed']:
            return EMPTY_PAGE, None
        append_record(self.record, dict(kind='printed', film_box=uid, calling_ae=str(event.assoc.requestor.ae_title),
                                        context=str(event.context.abstract_syntax), images=box['printed']))
        return 0x0000, None

    def on_delete(self, event):
        return 0x0000 if event.request.RequestedSOPInstanceUID in {**self.sessions, **self.boxes} else NO_SUCH_INSTANCE

    def on_get(self, event):
        reply = Dataset()
        reply.PrinterStatus, reply.PrinterStatusInfo = 'NORMAL', 'NORMAL'
        return 0x0000, reply


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--aet', required=True)
    parser.add_argument('--port', type=int, required=True)
    parser.add_argument('--record', required=True)
    args = parser.parse_args()
    printer = Printer(args.record)
    ae = AE(ae_title=args.aet)
    ae.acse_timeout = ae.dimse_timeout = ae.network_timeout = 30
    for uid in (GRAY_META, COLOR_META, PRINTER):
        ae.add_supported_context(uid, list(SYNTAXES))
    print(f'listening on {args.port}', flush=True)
    ae.start_server(('127.0.0.1', args.port), evt_handlers=[
        (evt.EVT_N_CREATE, printer.on_create), (evt.EVT_N_SET, printer.on_set), (evt.EVT_N_ACTION, printer.on_action),
        (evt.EVT_N_DELETE, printer.on_delete), (evt.EVT_N_GET, printer.on_get), (evt.EVT_ACCEPTED, printer.on_accepted)])


if __name__ == '__main__':
    main()
