using System;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using OpenCVBridge;
using Windows.Graphics.Imaging;
using Windows.Media.Capture;
using Windows.Media.Capture.Frames;
using Windows.Media.MediaProperties;
using Windows.Media.Ocr;

namespace OpenCVManager
{
    public sealed class VideoManager
    {
        string WinOCRResult = null;

        SoftwareBitmap LatestPreviewFrame;
        SoftwareBitmap LatestProcessedFrame;

        MediaCapture _mediaCapture = null;
        MediaFrameReader _reader = null;
        OpenCVHelper _helper;

        bool _ocrRunning = false;

        public VideoManager() { }

        public async void InitializeManager()
        {
            _helper = new OpenCVHelper();

            // Find the sources 
            var allGroups = await MediaFrameSourceGroup.FindAllAsync();
            var sourceGroups = allGroups.Select(g => new
            {
                Group = g,
                SourceInfo = g.SourceInfos.FirstOrDefault(i => i.SourceKind == MediaFrameSourceKind.Color)
            }).Where(g => g.SourceInfo != null).ToList();

            if (sourceGroups.Count == 0)
            {
                // No camera sources found
                return;
            }
            var selectedSource = sourceGroups.FirstOrDefault();

            // Initialize MediaCapture
            try
            {
                await InitializeMediaCaptureAsync(selectedSource.Group);
            }
            catch (Exception exception)
            {
                Debug.WriteLine("MediaCapture initialization error: " + exception.Message);
                await CleanupMediaCaptureAsync();
                return;
            }

            // Create the frame reader
            MediaFrameSource frameSource = _mediaCapture.FrameSources[selectedSource.SourceInfo.Id];
            var format = frameSource.SupportedFormats.OrderByDescending(x => x.VideoFormat.Width * x.VideoFormat.Height).FirstOrDefault();
            await frameSource.SetFormatAsync(format);
            BitmapSize size = new BitmapSize() // Choose a lower resolution to make the image processing more performant
            {
                Height = format.VideoFormat.Height,
                Width = format.VideoFormat.Width
            };
            _reader = await _mediaCapture.CreateFrameReaderAsync(frameSource, MediaEncodingSubtypes.Bgra8, size);
            _reader.FrameArrived += HandleFrameArrive;
            await _reader.StartAsync();
        }

        public async void DeinitializeManager()
        {
            await CleanupMediaCaptureAsync();
        }

        private async Task InitializeMediaCaptureAsync(MediaFrameSourceGroup sourceGroup)
        {
            if (_mediaCapture != null)
            {
                return;
            }

            _mediaCapture = new MediaCapture();
            var settings = new MediaCaptureInitializationSettings()
            {
                SourceGroup = sourceGroup,
                SharingMode = MediaCaptureSharingMode.ExclusiveControl,
                StreamingCaptureMode = StreamingCaptureMode.Video,
                MemoryPreference = MediaCaptureMemoryPreference.Cpu
            };
            await _mediaCapture.InitializeAsync(settings);
        }

        private async Task CleanupMediaCaptureAsync()
        {
            if (_mediaCapture != null)
            {
                await _reader.StopAsync();
                _reader.FrameArrived -= HandleFrameArrive;
                _reader.Dispose();
                _mediaCapture = null;
            }
        }

        private async void GetOCRAsync(SoftwareBitmap inputBitmap)
        {
            if (_ocrRunning) return;
            _ocrRunning = true;
            SoftwareBitmap localBitmap = SoftwareBitmap.Copy(inputBitmap);
            var ocrEngine = OcrEngine.TryCreateFromUserProfileLanguages();
            var recognizeAsync = await ocrEngine.RecognizeAsync(localBitmap);
            var str = new StringBuilder();
            foreach (var ocrLine in recognizeAsync.Lines)
                str.AppendLine(ocrLine.Text);
            var readText = str.ToString();
            if (readText != "")
                WinOCRResult = readText;
            _ocrRunning = false;
            return;
        }

        private void HandleFrameArrive(MediaFrameReader sender, MediaFrameArrivedEventArgs args)
        {
            var frame = sender.TryAcquireLatestFrame();
            if (frame != null)
            {
                SoftwareBitmap originalBitmap = null;
                var inputBitmap = frame.VideoMediaFrame?.SoftwareBitmap;
                if (inputBitmap != null)
                {
                    originalBitmap = SoftwareBitmap.Convert(inputBitmap, BitmapPixelFormat.Bgra8, BitmapAlphaMode.Premultiplied);

                    SoftwareBitmap outputBitmap = new SoftwareBitmap(BitmapPixelFormat.Bgra8, originalBitmap.PixelWidth, originalBitmap.PixelHeight, BitmapAlphaMode.Premultiplied);

                    _helper.Process(originalBitmap, outputBitmap);

                    var localBitmap = SoftwareBitmap.Copy(outputBitmap);
                    GetOCRAsync(localBitmap);

                    LatestPreviewFrame = SoftwareBitmap.Copy(originalBitmap);
                    LatestProcessedFrame = SoftwareBitmap.Copy(outputBitmap);
                    OnFrameProcessed(new FrameHandlerEventArgs());
                }
            }
        }

        public void OnFrameProcessed(FrameHandlerEventArgs e)
        {
            FrameProcessed?.Invoke(this, e);
        }

        public event Windows.Foundation.TypedEventHandler<object, FrameHandlerEventArgs> FrameProcessed;

        public SoftwareBitmap GetLatestPreviewFrame()
        {
            return LatestPreviewFrame;
        }

        public SoftwareBitmap GetLatestProcessedFrame()
        {
            return LatestProcessedFrame;
        }
       
    }

    public sealed class FrameHandlerEventArgs
    {
        string help = "I have absolutely no idea what I'm doing I'm so sorry";
        public FrameHandlerEventArgs() { }
    }
}