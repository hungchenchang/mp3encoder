{
  "targets": [
    {
      "target_name": "mp3encoder",
      "sources": [ "mp3encoder.cc", "ulaw.c" ],
      "include_dirs": [
        "./include/lame"
      ],
      "libraries": [
        "-L.lib", "-lmp3lame"
      ]
    }
  ]
}
