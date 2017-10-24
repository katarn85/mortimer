static GstCheckABIStruct list[] = {
  {"GstAllocTrace", sizeof (GstAllocTrace), 24},
  {"GstBinClass", sizeof (GstBinClass), 568},
  {"GstBin", sizeof (GstBin), 336},
  {"GstBufferClass", sizeof (GstBufferClass), 32},
  {"GstBuffer", sizeof (GstBuffer), 120},
  {"GstBusClass", sizeof (GstBusClass), 288},
  {"GstBus", sizeof (GstBus), 152},
  {"GstCaps", sizeof (GstCaps), 56},
  {"GstChildProxyInterface", sizeof (GstChildProxyInterface), 80},
  {"GstClockClass", sizeof (GstClockClass), 320},
  {"GstClockEntry", sizeof (GstClockEntry), 80},
  {"GstClock", sizeof (GstClock), 240},
  {"GstDebugCategory", sizeof (GstDebugCategory), 24},
  {"GstElementClass", sizeof (GstElementClass), 488},
  {"GstElementDetails", sizeof (GstElementDetails), 64},
  {"GstElementFactoryClass", sizeof (GstElementFactoryClass), 304},
  {"GstElementFactory", sizeof (GstElementFactory), 280},
  {"GstElement", sizeof (GstElement), 232},
  {"GstEventClass", sizeof (GstEventClass), 64},
  {"GstEvent", sizeof (GstEvent), 64},
  {"GstFormatDefinition", sizeof (GstFormatDefinition), 32},
  {"GstGhostPadClass", sizeof (GstGhostPadClass), 344},
  {"GstGhostPad", sizeof (GstGhostPad), 384},
  {"GstImplementsInterfaceClass", sizeof (GstImplementsInterfaceClass), 56},
  {"GstIndexAssociation", sizeof (GstIndexAssociation), 16},
  {"GstIndexClass", sizeof (GstIndexClass), 312},
  {"GstIndexEntry", sizeof (GstIndexEntry), 32},
  {"GstIndexFactoryClass", sizeof (GstIndexFactoryClass), 304},
  {"GstIndexFactory", sizeof (GstIndexFactory), 192},
  {"GstIndexGroup", sizeof (GstIndexGroup), 24},
  {"GstIndex", sizeof (GstIndex), 192},
  {"GstIterator", sizeof (GstIterator), 104},
  {"GstMessageClass", sizeof (GstMessageClass), 64},
  {"GstMessage", sizeof (GstMessage), 104},
  {"GstMiniObjectClass", sizeof (GstMiniObjectClass), 32},
  {"GstMiniObject", sizeof (GstMiniObject), 24},
  {"GstObjectClass", sizeof (GstObjectClass), 240},
  {"GstObject", sizeof (GstObject), 80},
  {"GstPadClass", sizeof (GstPadClass), 304},
  {"GstPad", sizeof (GstPad), 368},
  {"GstPadTemplateClass", sizeof (GstPadTemplateClass), 280},
  {"GstPadTemplate", sizeof (GstPadTemplate), 136},
  {"GstPadTemplate", sizeof (GstPadTemplate), 136},
  {"GstParamSpecFraction", sizeof (GstParamSpecFraction), 96},
  {"GstParamSpecMiniObject", sizeof (GstParamSpecMiniObject), 72},
  {"GstPipelineClass", sizeof (GstPipelineClass), 600},
  {"GstPipeline", sizeof (GstPipeline), 392},
  {"GstPluginClass", sizeof (GstPluginClass), 272},
  {"GstPluginDesc", sizeof (GstPluginDesc), 104},
  {"GstPluginFeatureClass", sizeof (GstPluginFeatureClass), 272},
  {"GstPluginFeature", sizeof (GstPluginFeature), 144},
  {"GstPlugin", sizeof (GstPlugin), 280},
  {"GstPresetInterface", sizeof (GstPresetInterface), 112},
  {"GstProxyPadClass", sizeof (GstProxyPadClass), 312},
  {"GstProxyPad", sizeof (GstProxyPad), 376},
  {"GstQueryClass", sizeof (GstQueryClass), 64},
  {"GstQuery", sizeof (GstQuery), 48},
  {"GstQueryTypeDefinition", sizeof (GstQueryTypeDefinition), 32},
  {"GstRegistryClass", sizeof (GstRegistryClass), 288},
  {"GstRegistry", sizeof (GstRegistry), 144},
  {"GstSegment", sizeof (GstSegment), 104},
  {"GstStaticCaps", sizeof (GstStaticCaps), 96},
  {"GstStaticPadTemplate", sizeof (GstStaticPadTemplate), 112},
  {"GstStructure", sizeof (GstStructure), 40},
  {"GstSystemClockClass", sizeof (GstSystemClockClass), 352},
  {"GstSystemClock", sizeof (GstSystemClock), 288},
  {"GstTagList", sizeof (GstTagList), 40},
  {"GstTagSetterIFace", sizeof (GstTagSetterIFace), 16},
  {"GstTaskClass", sizeof (GstTaskClass), 280},
  {"GstTask", sizeof (GstTask), 160},
  {"GstTaskPoolClass", sizeof (GstTaskPoolClass), 304},
  {"GstTaskPool", sizeof (GstTaskPool), 120},
  {"GstTaskThreadCallbacks", sizeof (GstTaskThreadCallbacks), 48},
  {"GstTraceEntry", sizeof (GstTraceEntry), 128},
  {"GstTrace", sizeof (GstTrace), 32},
  {"GstTypeFindFactoryClass", sizeof (GstTypeFindFactoryClass), 304},
  {"GstTypeFindFactory", sizeof (GstTypeFindFactory), 216},
  {"GstTypeFind", sizeof (GstTypeFind), 64},
#if !defined(GST_DISABLE_DEPRECATED) && !defined(GST_REMOVE_DEPRECATED)
  {"GstTypeNameData", sizeof (GstTypeNameData), 16},
#endif
  {"GstURIHandlerInterface", sizeof (GstURIHandlerInterface), 88},
  {"GstValueTable", sizeof (GstValueTable), 64},
#if !defined(GST_DISABLE_LOADSAVE) && !defined(GST_DISABLE_DEPRECATED) && !defined(GST_REMOVE_DEPRECATED)
  {"GstXML", sizeof (GstXML), 128} ,
  {"GstXMLClass", sizeof (GstXMLClass), 288} ,
#endif
  {NULL, 0, 0}
};