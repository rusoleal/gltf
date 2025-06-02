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
    
    var body: some View {
        VStack {
            /*Image(systemName: "globe")
                .imageScale(.large)
                .foregroundStyle(.tint)
            Text("Hello, world!")*/
            Button("Load glTF asset by URL", action: {
                print("load glTF button clicked.")
                showAlert = true
            })
            .alert("load glTF asset by URL", isPresented: $showAlert) {
                TextField("", text: $urlString)
                Button("Ok", action: {
                    print("trying to load asset: \(urlString)")

                    if let url = URL(string: urlString) {
                        do {
                            let contents = try String(contentsOf: url, encoding: .utf8)
                            
                            var pepe = gltf_binding(contents)
                        } catch {
                            
                        }
                    } else {
                        
                    }
                })
                Button("Cancel", role: .cancel, action: {
                    
                })
            }
            
        }
        .padding()
    }
}

#Preview {
    ContentView()
}
