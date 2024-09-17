/*import 'dart:io';
import 'package:flutter/material.dart';
import 'package:file_picker/file_picker.dart';
import 'package:http/http.dart' as http;
import 'package:permission_handler/permission_handler.dart';

void main() {
  runApp(MyApp());
}

class MyApp extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      home: FileUploadPage(),
    );
  }
}

class FileUploadPage extends StatefulWidget {
  @override
  _FileUploadPageState createState() => _FileUploadPageState();
}

class _FileUploadPageState extends State<FileUploadPage> {
  String? _ipAddress;
  String? _selectedFilePath;

  @override
  void initState() {
    super.initState();
    _checkPermission();
  }

  Future<void> _checkPermission() async {
    if (await Permission.storage.request().isGranted) {
      // Permission granted, continue
    } else {
      // Permission denied, show a message or handle accordingly
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
            content: Text('Storage permission is required to pick a file.')),
      );
    }
  }

  Future<void> _startFilePicker() async {
    FilePickerResult? result = await FilePicker.platform.pickFiles(
      type: FileType.custom,
      allowedExtensions: ['bin'],
    );

    if (result != null) {
      setState(() {
        _selectedFilePath = result.files.single.path;
      });
    }
  }

  Future<void> _uploadFile() async {
    if (_selectedFilePath == null ||
        _ipAddress == null ||
        _ipAddress!.isEmpty) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
            content: Text('Please select a file and enter the IP address.')),
      );
      return;
    }

    final uri = Uri.parse('http://$_ipAddress/upload');
    final request = http.MultipartRequest('POST', uri)
      ..files.add(await http.MultipartFile.fromPath(
        'file',
        _selectedFilePath!,
        filename: _selectedFilePath!.split('/').last,
      ));

    final response = await request.send();

    if (response.statusCode == 200) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('File uploaded successfully')),
      );
    } else {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('File upload failed')),
      );
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text('File Upload'),
      ),
      body: Padding(
        padding: EdgeInsets.all(16.0),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            TextField(
              decoration: InputDecoration(
                labelText: 'Enter ESP32 IP Address',
                border: OutlineInputBorder(),
              ),
              onChanged: (value) {
                _ipAddress = value;
              },
            ),
            SizedBox(height: 16),
            ElevatedButton(
              onPressed: _startFilePicker,
              child: Text('Select .bin File'),
            ),
            SizedBox(height: 16),
            ElevatedButton(
              onPressed: _uploadFile,
              child: Text('Upload File'),
            ),
            if (_selectedFilePath != null) ...[
              SizedBox(height: 16),
              Text('Selected file: ${_selectedFilePath!.split('/').last}'),
            ],
          ],
        ),
      ),
    );
  }
}*/

import 'package:flutter/material.dart';
import 'package:http/http.dart' as http;
import 'dart:convert';

import 'package:webappesp/firmware_version.dart';

class FirmwareList extends StatefulWidget {
  @override
  _FirmwareListState createState() => _FirmwareListState();
}

class _FirmwareListState extends State<FirmwareList> {
  List<FirmwareVersion> firmwareVersions = [];
  String? _ipAddress;

  @override
  void initState() {
    super.initState();
    fetchFirmwareVersions();
  }

  // Fetch the JSON file containing firmware versions
  Future<void> fetchFirmwareVersions() async {
    final response = await http.get(Uri.parse(
        'https://drive.google.com/uc?export=download&id=1R4iZKPCbdQDQ0JhU5KlgHnPGoNR7aliA'));

    if (response.statusCode == 200) {
      final data = json.decode(response.body);
      setState(() {
        // Parse the firmware versions from JSON
        firmwareVersions = (data['firmware_versions'] as List)
            .map((item) => FirmwareVersion.fromJson(item))
            .toList();
      });
    } else {
      print('Failed to load firmware versions');
    }
  }

  Future<void> _sendFirmwareUrl(String url) async {
    if (_ipAddress == null || _ipAddress!.isEmpty) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Please enter the ESP32 IP address')),
      );
      return;
    }

    final response = await http.post(
      Uri.parse('http://$_ipAddress/update-firmware'),
      body: json.encode({'url': url}),
      headers: {'Content-Type': 'application/json'},
    );
    print(response.statusCode);
    if (response.statusCode == 200) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Firmware URL sent successfully')),
      );
    } else {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Failed to send firmware URL')),
      );
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Firmware Versions')),
      body: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          children: [
            TextField(
              decoration: const InputDecoration(
                labelText: 'Enter ESP32 IP Address',
                border: OutlineInputBorder(),
              ),
              onChanged: (value) {
                _ipAddress = value;
              },
            ),
            SizedBox(height: 16),
            Expanded(
              child: firmwareVersions.isEmpty
                  ? const Center(child: CircularProgressIndicator())
                  : ListView.builder(
                      itemCount: firmwareVersions.length,
                      itemBuilder: (context, index) {
                        final firmware = firmwareVersions[index];
                        return ListTile(
                          title: Text('${firmware.displayVersion}'),
                          onTap: () {
                            _sendFirmwareUrl(firmware.url);
                          },
                        );
                      },
                    ),
            ),
          ],
        ),
      ),
    );
  }
}

void main() {
  runApp(MaterialApp(
    home: FirmwareList(),
  ));
}
