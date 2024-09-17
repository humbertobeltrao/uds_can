// Model class to represent a firmware version
class FirmwareVersion {
  final String url;
  final String displayVersion;

  FirmwareVersion({required this.url, required this.displayVersion});

  // Parse JSON to create a FirmwareVersion object
  factory FirmwareVersion.fromJson(Map<String, dynamic> json) {
    return FirmwareVersion(
      url: json['url'],
      displayVersion: json['display_version'],
    );
  }
}
