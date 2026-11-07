"""Independent encoders, decoders and pixel oracles for the corpus (#1318).

JPEG, JPEG-LS and RLE use DCMTK, which shares no code with the PACS codecs.
JPEG 2000 uses OpenJPEG tools, with FFmpeg's own decoder and encoder as a second
implementation for unsigned grayscale, since PACS links the OpenJPEG library.
No independent HTJ2K encoder is packaged: ojph_compress shares OpenJPH with PACS
and is independent only of the PACS wrapper, while OpenJPEG and FFmpeg decode
HTJ2K with their own code. ojph_compress misreads negative samples in raw files,
and opj_compress reads signed 8-bit ones through plain char, unsigned on aarch64.
For those formats the input comes from PACS after OpenJPEG has confirmed that the
stream holds the source samples. Each case records which encoder was used.
FFmpeg 7.1 is not exact on its own 12-bit streams, so it checks 8 and 16 bits only.
"""
from io import BytesIO
from pathlib import Path

import numpy as np
import pydicom
from pydicom.encaps import encapsulate, generate_pixel_data_frame
from pydicom.filebase import DicomBytesIO
from pydicom.filewriter import write_file_meta_info
from pydicom.uid import UID

from corpus import LOSSY_BOUNDS, LOSSY_QUALITY, PIXEL_FORMATS, SYNTAX_BY_UID
from corpus_fixtures import native_bytes, pixel_array

DCMTK_ENCODERS = {
    '1.2.840.10008.1.2.4.50': ['dcmcjpeg', '+eb'],
    '1.2.840.10008.1.2.4.70': ['dcmcjpeg', '+e1'],
    '1.2.840.10008.1.2.4.80': ['dcmcjpls', '+el'],
    '1.2.840.10008.1.2.4.81': ['dcmcjpls', '+en'],
    '1.2.840.10008.1.2.5': ['dcmcrle'],
}
DCMTK_DECODERS = {
    '1.2.840.10008.1.2.4.50': 'dcmdjpeg',
    '1.2.840.10008.1.2.4.70': 'dcmdjpeg',
    '1.2.840.10008.1.2.4.80': 'dcmdjpls',
    '1.2.840.10008.1.2.4.81': 'dcmdjpls',
    '1.2.840.10008.1.2.5': 'dcmdrle',
}
OPENJPEG_SYNTAXES = {'1.2.840.10008.1.2.4.90', '1.2.840.10008.1.2.4.91', '1.2.840.10008.1.2.4.201',
                     '1.2.840.10008.1.2.4.202', '1.2.840.10008.1.2.4.203'}
ALLOWED_COLOR = {
    '1.2.840.10008.1.2.4.50': {'YBR_FULL_422', 'YBR_FULL', 'RGB'},
    '1.2.840.10008.1.2.4.80': {'RGB'},
    '1.2.840.10008.1.2.4.81': {'RGB'},
    '1.2.840.10008.1.2.4.90': {'RGB', 'YBR_RCT'},
    '1.2.840.10008.1.2.4.91': {'RGB', 'YBR_ICT', 'YBR_RCT'},
    '1.2.840.10008.1.2.5': {'RGB'},
    '1.2.840.10008.1.2.4.201': {'RGB', 'YBR_RCT'},
    '1.2.840.10008.1.2.4.202': {'RGB', 'YBR_RCT'},
    '1.2.840.10008.1.2.4.203': {'RGB', 'YBR_RCT', 'YBR_ICT'},
}


def stored_values(array, format_):
    """Interpret samples by Bits Stored, so unused high bits cannot hide or fake a match."""
    values = np.asarray(array).astype('i8') & ((1 << format_.bits_stored) - 1)
    if format_.signed:
        sign = 1 << (format_.bits_stored - 1)
        values = np.where(values >= sign, values - (1 << format_.bits_stored), values)
    return values


def source_values(format_, frames):
    return stored_values(pixel_array(format_, frames), format_)


def native_values(ds, format_, frames):
    """Read a native dataset's Pixel Data as (frames, rows, columns[, samples])."""
    kind = {8: 'u1', 16: '<u2'}[int(ds.BitsAllocated)]
    data = np.frombuffer(bytes(ds.PixelData), dtype=kind)
    samples, rows, columns = int(ds.SamplesPerPixel), int(ds.Rows), int(ds.Columns)
    expected = frames * rows * columns * samples
    if data.size < expected or data.size > expected + 1:
        raise ValueError(f'Native Pixel Data holds {data.size} samples, expected {expected}')
    data = data[:expected]
    if samples == 1:
        return stored_values(data.reshape(frames, rows, columns), format_)
    if int(ds.get('PlanarConfiguration', 0)) == 1:
        return stored_values(np.moveaxis(data.reshape(frames, samples, rows, columns), 1, -1), format_)
    return stored_values(data.reshape(frames, rows, columns, samples), format_)


def to_rgb(values, photometric):
    if photometric in ('RGB', 'MONOCHROME1', 'MONOCHROME2'):
        return values
    if photometric in ('YBR_FULL', 'YBR_FULL_422'):
        y, cb, cr = [values[..., channel].astype('f8') for channel in range(3)]
        rgb = np.stack([y + 1.402 * (cr - 128), y - 0.344136 * (cb - 128) - 0.714136 * (cr - 128),
                        y + 1.772 * (cb - 128)], axis=-1)
        return np.clip(np.rint(rgb), 0, 255).astype('i8')
    raise ValueError(f'Cannot normalize Photometric Interpretation {photometric}')


def require_exact(actual, expected, label):
    if actual.shape != expected.shape:
        raise ValueError(f'{label}: shape {actual.shape}, expected {expected.shape}')
    if not np.array_equal(actual, expected):
        index = tuple(int(i) for i in np.argwhere(actual != expected)[0])
        raise ValueError(f'{label}: frame {index[0]} sample {index[1:]} is {int(actual[index])}, '
                         f'expected {int(expected[index])}; {int((actual != expected).sum())} samples differ')
    return f'exact {expected.size} samples in {expected.shape[0]} frame(s)'


def measure(actual, expected, format_):
    if actual.shape != expected.shape:
        raise ValueError(f'shape {actual.shape}, expected {expected.shape}')
    error = actual.astype('f8') - expected.astype('f8')
    peak = (1 << format_.bits_stored) - 1
    mse = float(np.mean(error ** 2))
    return dict(max_abs_error=int(np.abs(error).max()),
                psnr_db=float('inf') if mse == 0 else 10 * float(np.log10(peak ** 2 / mse)))


def require_lossy_bound(actual, expected, format_, syntax_uid):
    """Check the bound declared in corpus.LOSSY_BOUNDS, computed over every frame."""
    bound, metrics = LOSSY_BOUNDS[syntax_uid], measure(actual, expected, format_)
    if 'max_abs_error' in bound and metrics['max_abs_error'] > bound['max_abs_error']:
        raise ValueError(f'max error {metrics["max_abs_error"]} exceeds {bound["max_abs_error"]}')
    if 'min_psnr_db' in bound and metrics['psnr_db'] < bound['min_psnr_db']:
        raise ValueError(f'PSNR {metrics["psnr_db"]:.2f} dB is below {bound["min_psnr_db"]}')
    return f'max error {metrics["max_abs_error"]}, PSNR {metrics["psnr_db"]:.2f} dB within {bound}'


class Codecs:
    """Run the external tools through the caller's recorded command runner."""

    def __init__(self, command, work, dcm_conv='dcm_conv'):
        self.command = command
        self.dcm_conv = dcm_conv
        self.work = Path(work)
        self.work.mkdir(parents=True, exist_ok=True)
        self.cache = {}
        self.counter = 0

    def scratch(self, suffix):
        self.counter += 1
        return self.work/f'{self.counter:06d}{suffix}'

    def write_source(self, format_key, frames, base_ds):
        """Write a native Secondary Capture carrier for the external tools."""
        format_ = PIXEL_FORMATS[format_key]
        path = self.scratch('.dcm')
        base_ds.Rows, base_ds.Columns = format_.rows, format_.columns
        base_ds.PixelData = native_bytes(format_, frames)
        base_ds.save_as(path, write_like_original=False)
        return path

    def frames_from_file(self, path, frames):
        ds = pydicom.dcmread(path)
        return list(generate_pixel_data_frame(ds.PixelData, frames)), ds

    def raw_frame(self, format_, frames, index):
        """Planar little-endian samples for one frame, as the raw-file tools read them."""
        array = pixel_array(format_, frames)[index]
        if format_.samples > 1:
            array = np.moveaxis(array, -1, 0)
        path = self.scratch('.rawl')
        path.write_bytes(array.tobytes())
        return path

    def openjpeg_encode(self, format_, frames, lossy):
        output = []
        for index in range(frames):
            raw, stream = self.raw_frame(format_, frames, index), self.scratch('.j2k')
            sign = 's' if format_.signed else 'u'
            spec = f'{format_.columns},{format_.rows},{format_.samples},{format_.bits_stored},{sign}'
            rate = ['-I', '-r', '5'] if lossy else []
            self.command(['opj_compress', '-i', raw, '-F', spec, '-o', stream, *rate])
            output.append(stream.read_bytes())
        return output

    def ffmpeg_encode(self, format_, frames):
        """A J2K encoder that shares no code with OpenJPEG (unsigned grayscale only)."""
        pixel = {8: 'gray', 12: 'gray12le', 16: 'gray16le'}[format_.bits_stored]
        output = []
        for index in range(frames):
            raw, stream = self.raw_frame(format_, frames, index), self.scratch('.j2k')
            self.command(['ffmpeg', '-hide_banner', '-loglevel', 'error', '-y', '-f', 'rawvideo',
                          '-pix_fmt', pixel, '-s', f'{format_.columns}x{format_.rows}', '-i', raw,
                          '-c:v', 'jpeg2000', '-pred', 'dwt53', '-format', 'j2k',
                          '-f', 'image2', '-update', '1', stream])
            output.append(stream.read_bytes())
        return output

    def ojph_encode(self, format_, frames, syntax_uid):
        lossy = syntax_uid == '1.2.840.10008.1.2.4.203'
        output = []
        for index in range(frames):
            raw, stream = self.raw_frame(format_, frames, index), self.scratch('.j2c')
            yuv = raw.with_suffix('.yuv')
            raw.rename(yuv)
            count = format_.samples
            arguments = ['ojph_compress', '-i', yuv, '-o', stream,
                         '-dims', f'{{{format_.columns},{format_.rows}}}', '-num_comps', count,
                         '-signed', ','.join(['true' if format_.signed else 'false'] * count),
                         '-bit_depth', ','.join([str(format_.bits_stored)] * count),
                         '-downsamp', ','.join(['{1,1}'] * count), '-prog_order', 'RPCL']
            arguments += ['-reversible', 'false', '-qstep', '0.002'] if lossy else ['-reversible', 'true']
            self.command(arguments)
            output.append(stream.read_bytes())
        return output

    def pacs_verified(self, format_key, frames, syntax_uid, base_ds):
        """Use a PACS stream only after OpenJPEG confirms that it holds the source samples."""
        format_ = PIXEL_FORMATS[format_key]
        source, encoded = self.write_source(format_key, frames, base_ds), self.scratch('.dcm')
        quality = ['-q', LOSSY_QUALITY[syntax_uid]] if syntax_uid in LOSSY_QUALITY else []
        self.command([self.dcm_conv, source, encoded, '-t', syntax_uid, *quality, '--no-progress'])
        ds = pydicom.dcmread(encoded)
        actual = self.openjpeg_decode(ds, format_, frames, syntax_uid)
        expected = source_values(format_, frames)
        if SYNTAX_BY_UID[syntax_uid].lossy:
            require_lossy_bound(actual, expected, format_, syntax_uid)
        else:
            require_exact(actual, expected, 'OpenJPEG check of the PACS stream')
        attributes = {name: ds.get(name) for name in ('PhotometricInterpretation', 'LossyImageCompression',
                      'LossyImageCompressionRatio', 'LossyImageCompressionMethod') if name in ds}
        return list(generate_pixel_data_frame(ds.PixelData, frames)), attributes, 'dcm_conv verified by opj_decompress'

    def encode(self, format_key, frames, syntax_uid, base_ds):
        """Return independently encoded frames, attribute changes and the encoder name."""
        key = (format_key, frames, syntax_uid)
        if key in self.cache:
            return self.cache[key]
        format_ = PIXEL_FORMATS[format_key]
        syntax = SYNTAX_BY_UID[syntax_uid]
        if syntax_uid in DCMTK_ENCODERS:
            source, encoded = self.write_source(format_key, frames, base_ds), self.scratch('.dcm')
            self.command([*DCMTK_ENCODERS[syntax_uid], source, encoded])
            items, ds = self.frames_from_file(encoded, frames)
            attributes = {name: ds.get(name) for name in ('PhotometricInterpretation', 'PlanarConfiguration',
                          'LossyImageCompression', 'LossyImageCompressionRatio', 'LossyImageCompressionMethod')
                          if name in ds}
            encoder = DCMTK_ENCODERS[syntax_uid][0]
        elif format_.signed and (syntax.tool_family == 'htj2k' or format_.bits_stored == 8):
            items, attributes, encoder = self.pacs_verified(format_key, frames, syntax_uid, base_ds)
        elif syntax.tool_family == 'jpeg2000':
            items, attributes, encoder = self.openjpeg_encode(format_, frames, syntax.lossy), {}, 'opj_compress'
        else:
            items, attributes, encoder = self.ojph_encode(format_, frames, syntax_uid), {}, 'ojph_compress'
        if syntax.lossy and 'LossyImageCompression' not in attributes:
            bound = LOSSY_BOUNDS[syntax_uid]
            original = format_.rows * format_.columns * frames * format_.samples * format_.bits_allocated // 8
            attributes.update(LossyImageCompression='01', LossyImageCompressionMethod=bound['method'],
                              LossyImageCompressionRatio=f'{original / max(1, sum(map(len, items))):.2f}')
        result = (items, attributes, encoder)
        self.cache[key] = result
        return result

    def openjpeg_decode(self, ds, format_, frames, syntax_uid):
        output = []
        for item in generate_pixel_data_frame(ds.PixelData, frames):
            stream, raw = self.scratch('.j2c'), self.scratch('.rawl')
            stream.write_bytes(item)
            self.command(['opj_decompress', '-i', stream, '-o', raw])
            kind = 'u1' if format_.bits_allocated == 8 else ('<i2' if format_.signed else '<u2')
            # OpenJPEG clips ringing to the declared range and writes Bits Stored patterns.
            data = np.frombuffer(raw.read_bytes(), dtype=kind)
            if format_.samples > 1:
                data = np.moveaxis(data.reshape(format_.samples, format_.rows, format_.columns), 0, -1)
            else:
                data = data.reshape(format_.rows, format_.columns)
            output.append(data)
        return stored_values(np.stack(output), format_)

    def ffmpeg_decode(self, ds, format_, frames):
        """Second, unrelated decoder for unsigned grayscale JPEG 2000 and HTJ2K."""
        pixel = {8: 'gray', 12: 'gray12le', 16: 'gray16le'}[format_.bits_stored]
        output = []
        for item in generate_pixel_data_frame(ds.PixelData, frames):
            stream, raw = self.scratch('.j2c'), self.scratch('.raw')
            stream.write_bytes(item)
            self.command(['ffmpeg', '-hide_banner', '-loglevel', 'fatal', '-y', '-c:v', 'jpeg2000', '-i', stream,
                          '-f', 'rawvideo', '-pix_fmt', pixel, raw])
            kind = 'u1' if format_.bits_allocated == 8 else '<u2'
            output.append(np.frombuffer(raw.read_bytes(), dtype=kind).reshape(format_.rows, format_.columns))
        return stored_values(np.stack(output), format_)

    def decode(self, path, format_, frames):
        """Decode a compressed file independently; return RGB-normalized stored values."""
        ds = pydicom.dcmread(path)
        syntax_uid = str(ds.file_meta.TransferSyntaxUID)
        if syntax_uid in DCMTK_DECODERS:
            decoded = self.scratch('.dcm')
            self.command([DCMTK_DECODERS[syntax_uid], path, decoded])
            native = pydicom.dcmread(decoded)
            return to_rgb(native_values(native, format_, frames), str(native.PhotometricInterpretation))
        return self.openjpeg_decode(ds, format_, frames, syntax_uid)


def save_any(ds, path):
    """Write a file in any syntax; pydicom 2.4 refuses UIDs it does not know (HTJ2K, HEVC, ...)."""
    uid = str(ds.file_meta.TransferSyntaxUID)
    if UID(uid).is_transfer_syntax:
        ds.save_as(path, write_like_original=False)
        return
    temporary = Path(str(path)+'.tmp')
    ds.file_meta.TransferSyntaxUID = UID('1.2.840.10008.1.2.4.90')
    ds.save_as(temporary, write_like_original=False)
    data = temporary.read_bytes()
    temporary.unlink()
    meta_end = 132+12+int.from_bytes(data[140:144], 'little')
    meta = pydicom.dcmread(BytesIO(data), stop_before_pixels=True).file_meta
    meta.TransferSyntaxUID = UID(uid)
    stream = DicomBytesIO()
    stream.is_little_endian, stream.is_implicit_VR = True, False
    write_file_meta_info(stream, meta, enforce_standard=True)
    Path(path).write_bytes(data[:132]+stream.getvalue()+data[meta_end:])
    ds.file_meta.TransferSyntaxUID = UID(uid)


def encapsulated(native_ds, items, attributes, syntax_uid):
    """Copy a native dataset and place independently encoded frames in it unchanged."""
    ds = native_ds.copy() if hasattr(native_ds, 'copy') else native_ds
    ds.file_meta = native_ds.file_meta.copy() if hasattr(native_ds.file_meta, 'copy') else native_ds.file_meta
    for name, value in attributes.items():
        setattr(ds, name, value)
    ds.file_meta.TransferSyntaxUID = UID(syntax_uid)
    ds.PixelData = encapsulate(items, has_bot=len(items) > 1)
    ds['PixelData'].VR = 'OB'
    ds['PixelData'].is_undefined_length = True
    ds.is_little_endian, ds.is_implicit_VR = True, False
    return ds
