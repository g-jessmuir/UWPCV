using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Controls.Primitives;
using Windows.UI.Xaml.Data;
using Windows.UI.Xaml.Input;
using Windows.UI.Xaml.Media;
using Windows.UI.Xaml.Navigation;
using OpenCVManager;

// The Blank Page item template is documented at https://go.microsoft.com/fwlink/?LinkId=402352&clcid=0x409

namespace CVDebug
{
    /// <summary>
    /// An empty page that can be used on its own or navigated to within a Frame.
    /// </summary>
    public sealed partial class MainPage : Page
    {
        private FrameRenderer _previewRenderer = null;
        private FrameRenderer _processedRenderer = null;
        private VideoManager _videoManager = null;

        public MainPage()
        {
            this.InitializeComponent();

            _videoManager = new VideoManager();
            _videoManager.FrameProcessed += handleFrames;

            _previewRenderer = new FrameRenderer(PreviewImage);
            _processedRenderer = new FrameRenderer(ProcessedImage);
        }

        protected override void OnNavigatedTo(NavigationEventArgs e)
        {
            _videoManager.InitializeManager();
        }

        protected override void OnNavigatedFrom(NavigationEventArgs args)
        {
            _videoManager.DeinitializeManager();
        }

        private void handleFrames(object sender, FrameHandlerEventArgs e)
        {
            _previewRenderer.RenderFrame(_videoManager.GetLatestPreviewFrame());
            _processedRenderer.RenderFrame(_videoManager.GetLatestProcessedFrame());
        }
    }
}
