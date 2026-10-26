// SPDX-License-Identifier: MIT OR Apache-2.0
import COzen

#if canImport(Glibc)
import Glibc
#elseif canImport(Darwin)
import Darwin
#endif

private func ozenAlloc(_ ctx: UnsafeMutableRawPointer?, _ size: Int) -> UnsafeMutableRawPointer? {
  return malloc(size)
}

private func ozenRelease(_ ctx: UnsafeMutableRawPointer?, _ ptr: UnsafeMutableRawPointer?, _ size: Int) {
  free(ptr)
}

public enum Ozen {
  public static func build(sources: [(path: String, text: String)], kitchen: String?, options: String?, entry: String?,
                           emit: UInt32) -> String? {
    var cfg = recipe_default_config()
    cfg.alloc = ozenAlloc
    cfg.release = ozenRelease
    cfg.emit = emit
    let paths = sources.map { strdup($0.path) }
    let texts = sources.map { Array($0.text.utf8) }
    defer { paths.forEach { free($0) } }
    return texts.withUnsafeBufferPointers { textPointers in
      var list = [recipe_source]()
      for (i, text) in textPointers.enumerated() {
        list.append(recipe_source(path: paths[i], text: text.baseAddress.map { UnsafeRawPointer($0).assumingMemoryBound(to: CChar.self) }, len: texts[i].count))
      }
      return withOptionalCString(entry) { entryPointer in
        cfg.entry = entryPointer
        return withOptionalBytes(kitchen) { kitchenPointer, kitchenCount in
          withOptionalBytes(options) { optionsPointer, optionsCount in
            guard let out = recipe_build(&cfg, list, list.count, kitchenPointer, kitchenCount, optionsPointer, optionsCount)
            else {
              return nil
            }
            let result = String(cString: out)
            recipe_free_string(&cfg, out)
            return result
          }
        }
      }
    }
  }
}

private func withOptionalCString<R>(_ s: String?, _ body: (UnsafePointer<CChar>?) -> R) -> R {
  guard let s = s else {
    return body(nil)
  }
  return s.withCString { body($0) }
}

private func withOptionalBytes<R>(_ s: String?, _ body: (UnsafePointer<CChar>?, Int) -> R) -> R {
  guard let s = s else {
    return body(nil, 0)
  }
  let bytes = Array(s.utf8)
  return bytes.withUnsafeBufferPointer { buffer in
    body(buffer.baseAddress.map { UnsafeRawPointer($0).assumingMemoryBound(to: CChar.self) }, bytes.count)
  }
}

extension Array where Element == [UInt8] {
  fileprivate func withUnsafeBufferPointers<R>(_ body: ([UnsafeBufferPointer<UInt8>]) -> R) -> R {
    var pointers = [UnsafeBufferPointer<UInt8>]()
    var copies = [UnsafeMutablePointer<UInt8>]()
    for element in self {
      let copy = UnsafeMutablePointer<UInt8>.allocate(capacity: Swift.max(element.count, 1))
      copy.initialize(from: element, count: element.count)
      copies.append(copy)
      pointers.append(UnsafeBufferPointer(start: copy, count: element.count))
    }
    defer { copies.forEach { $0.deallocate() } }
    return body(pointers)
  }
}
