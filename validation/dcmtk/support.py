"""Discover declared protocol contracts without treating declarations as passes."""
import hashlib
from pathlib import Path
import re


# Storage classes and transfer syntaxes belong to the corpus (#1318). Query/Retrieve and
# Verification negative paths are #1319; normalized services are #1320.
OWNER_BY_SECTION = {'3.1 Verification': 1319, '3.3 Query/Retrieve': 1319, '3.4 Modality Worklist': 1320,
                    '3.5 Modality Performed Procedure Step (MPPS)': 1320, '3.6 Storage Commitment Push Model': 1320,
                    'SOP Classes': 1320}


def declared_support(root):
    root = Path(root)
    document = root/'docs/DICOM_CONFORMANCE_STATEMENT.md'
    text = document.read_text()
    sops, syntaxes = [], []
    header, section = [], ''
    for line in text.splitlines():
        if line.startswith('#'):
            section = line.lstrip('# ').strip()
        if not line.startswith('|'):
            continue
        cells = [cell.strip() for cell in line.strip('|').split('|')]
        if 'UID' in cells:
            header = cells
            continue
        if len(cells) != len(header) or 'UID' not in header:
            continue
        row = dict(zip(header, cells))
        if not re.fullmatch(r'[0-9]+(?:\.[0-9]+)+', row['UID']):
            continue
        if 'SOP Class' in row:
            sops.append(dict(name=row['SOP Class'], uid=row['UID'], section=section,
                             roles=[role for role in ('SCP','SCU') if row.get(role) == 'Yes']))
        elif 'Transfer Syntax' in row:
            syntaxes.append(dict(name=row['Transfer Syntax'], uid=row['UID'],
                                 network=row['PACS_SCP negotiation'],
                                 file_or_codec=row.get('Part 10 files',row.get('Pixel data encoding and decoding'))))
    if not sops or not syntaxes:
        raise ValueError('conformance tables did not yield SOP and transfer-syntax contracts')
    sources = [document, root/'src/services/sop_class_registry.cpp',
               root/'src/encoding/transfer_syntax.cpp',
               root/'include/kcenon/pacs/network/detail/scp_transfer_syntaxes.h',
               root/'cmake/options.cmake', root/'web/package.json']
    # Preserve source identities for review of documentation/registry divergence.
    # This is a declaration inventory; runtime support/valid IOD combinations are
    # evaluated by their explicit external cases, never inferred from UID presence.
    # "assigned" names an executing job (component.corpus, component.qr); only its results pass anything.
    provenance = {str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest()
                  for path in sources}
    cases = []
    for sop in sops:
        owner = OWNER_BY_SECTION.get(sop['section'], 1318)
        for role in sop['roles']:
            for condition in ('positive','negative'):
                cases.append(dict(case_id=f'support.sop.{sop["uid"]}.{role}.{condition}',
                    feature_id='association' if sop['uid']=='1.2.840.10008.1.1' else 'dicom',
                    sop_class_uids=[sop['uid']], roles=[role], syntaxes=[],
                    support_status='declared-supported', condition=condition,
                    expected_outcome='valid exchange and exact object/state' if condition=='positive'
                    else 'specified rejection with no unintended object/state change',
                    fixture='valid IOD/role fixture or designated invalid request',
                    oracle='independent peer responses and object/state comparison',
                    prerequisites=['qualified independent peer','valid SOP/syntax/pixel-format combination'],
                    execution_lane='full-dicom', owner_issue=owner,
                    assignment_status='assigned' if owner in (1318, 1319) else 'unverified',
                    evidence_schema='case result plus independent peer/object evidence'))
    for syntax in syntaxes:
        for boundary in ('network','file_or_codec'):
            supported = (syntax[boundary]=='Accepted' if boundary=='network'
                         else not syntax[boundary].startswith(('Not supported','Not implemented')))
            cases.append(dict(case_id=f'support.syntax.{syntax["uid"]}.{boundary}',
                feature_id='encoding', sop_class_uids=[], roles=['SCP'] if boundary=='network' else ['file'],
                syntaxes=[syntax['uid']], support_status='declared-supported' if supported else 'unsupported',
                condition='positive' if supported else 'negative',
                expected_outcome='matching representation and full content integrity' if supported
                else 'explicit rejection/error without data corruption',
                fixture='valid IOD and syntax-specific bytes; independent codec fixture when required',
                oracle='negotiated syntax, file metadata and independent decoded pixels/structure',
                prerequisites=[syntax['file_or_codec']], execution_lane='full-dicom', owner_issue=1318,
                assignment_status='assigned', evidence_schema='case result plus wire/file/oracle evidence'))
    return dict(schema_version=1, sources=provenance, sop_classes=sops, transfer_syntaxes=syntaxes,
                cases=cases, combination_policy='Only valid SOP/role/syntax/pixel-format combinations; '
                'no Cartesian product. Forwarding and codec availability are distinct.',
                additional_discovery=[
                    dict(domain='web',owner_issue=1322,status='unverified',source='web/package.json'),
                    dict(domain='modules',owner_issue=1327,status='unverified',source='.github/workflows/modules.yml'),
                    dict(domain='installed-consumer',owner_issue=1327,status='unverified',source='.github/workflows/source-install.yml'),
                    dict(domain='enabled-sdk-builds',owner_issue=1324,status='unverified',source='cmake/options.cmake')])
