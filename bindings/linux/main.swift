// SPDX-License-Identifier: MIT OR Apache-2.0
import Foundation

let args = CommandLine.arguments
let outDir = args[1]
for (i, path) in args.dropFirst(2).enumerated() {
  let data = FileManager.default.contents(atPath: path) ?? Data()
  let text = String(decoding: data, as: UTF8.self)
  let name = (path as NSString).lastPathComponent
  let result = Ozen.build(sources: [(path: name, text: text)], kitchen: nil, options: nil, entry: nil, emit: 1 | 2 | 4 | 16) ?? ""
  FileManager.default.createFile(atPath: "\(outDir)/\(i).json", contents: Data(result.utf8))
}
