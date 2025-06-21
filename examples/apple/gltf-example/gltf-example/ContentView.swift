//
//  ContentView.swift
//  gltf-example
//
//  Created by Ruben Leal on 24/5/25.
//

import SwiftUI

struct ContentView: View {
    
    @State private var showAlert = false
    @State private var urlString: String = ""
    @State private var asset: gltf_binding? = nil
    @State private var message: String = "Load a glTF asset..."
    
    var body: some View {
        VStack {
            /*Image(systemName: "globe")
                .imageScale(.large)
                .foregroundStyle(.tint)
            Text("Hello, world!")*/
            Button("Clear", action: {
                asset = nil
                print("clear button clicked.")
            })
            Button("Load gltf/glb asset by URL", action: {
                print("load glTF button clicked.")
                showAlert = true
            })
            .alert("load glTF asset by URL", isPresented: $showAlert) {
                TextField("", text: $urlString)
                Button("Ok", action: {
                    print("trying to load asset: \(urlString)")

                    if let url = URL(string: urlString) {
                        if (url.pathExtension.lowercased() == "gltf") {
                            do {
                                let contents = try String(contentsOf: url, encoding: .utf8)
                                try ObjC.catchException {
                                    asset = gltf_binding(string: contents)
                                    message = asset!.toString()
                                }
                            } catch {
                                message = "\(error)"
                            }
                        } else if (url.pathExtension.lowercased() == "glb") {
                            
                            URLSession.shared.dataTask(with: url) { (data, res, error) in
                                
                                guard let data = data else {
                                      return
                                }

                                do {
                                    try ObjC.catchException {
                                        asset = gltf_binding(nsData: data)
                                        message = asset!.toString()
                                    }
                                } catch {
                                    message = "\(error)"
                                }
                            }.resume()
                            
                        } else {
                            message = "unsupported format"
                        }
                    }
                })
                Button("Cancel", role: .cancel, action: {
                })
            }
            Text(message)
            
        }
        .padding()
    }
}

#Preview {
    ContentView()
}
