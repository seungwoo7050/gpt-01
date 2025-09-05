# Extension 09: End-to-End Client Demo

## 1. Objective

This guide provides a blueprint for creating a minimal client application (e.g., using Unity) to visually demonstrate and validate the server's end-to-end functionality. The goal is to create a client that can:
1.  Connect and log in to the server.
2.  Enter the matchmaking queue.
3.  Receive a match notification and connect to the assigned room (conceptually).
4.  Render the game state (other players) based on snapshot and delta updates from the server.

This guide is conceptual and focuses on the client-side logic required to interact with the server features we've designed. No server code will be changed.

## 2. Analysis of Current Implementation

There is no client implementation. The `load_test_client` is a command-line tool for performance testing and cannot visualize the game state. This guide outlines a new project.

## 3. Proposed Implementation (Unity & C#)

This implementation assumes you are creating a new Unity project.

### Step 3.1: Setting up Protobuf for C#

1.  **Get Protobuf Files**: Copy all your `.proto` files from the server project into a `Protos` folder in your Unity project.
2.  **Install gRPC/Protobuf Tools**: In Unity, use the NuGet package manager (or a Unity-specific asset like `Grpc.Tools`) to install `Google.Protobuf`, `Grpc.Net.Client`, and `Grpc.Tools`.
3.  **Compile Protos**: Configure your Unity project build process to automatically run `protoc` (the protobuf compiler) on your `.proto` files to generate C# classes. The command would look something like this:
    ```sh
    protoc --csharp_out=Generated/ --proto_path=Protos/ protos/*.proto
    ```
    This will create C# versions of your `LoginRequest`, `MatchFoundNotification`, `RoomStateSnapshot`, etc., messages.

### Step 3.2: Network Manager

Create a `NetworkManager.cs` singleton in Unity to handle all communication with the server.

**Conceptual Code (`NetworkManager.cs`):**
```csharp
using System.Net.Sockets;
using System.Net.Security;
using Google.Protobuf;

public class NetworkManager 
{
    private TcpClient _tcpClient;
    private SslStream _sslStream;
    private CancellationTokenSource _cancellationTokenSource;

    public async Task ConnectAsync(string host, int port) 
    {
        _tcpClient = new TcpClient();
        await _tcpClient.ConnectAsync(host, port);
        
        _sslStream = new SslStream(_tcpClient.GetStream(), false, 
            (sender, certificate, chain, sslPolicyErrors) => true); // Trust self-signed cert for demo
        
        await _sslStream.AuthenticateAsClientAsync(host);
        _cancellationTokenSource = new CancellationTokenSource();
        _ = Task.Run(() => ReadLoop(_cancellationTokenSource.Token));
    }

    public void Send<T>(T message) where T : IMessage 
    {
        // This needs to serialize the message using the same PacketHeader format as the server.
        // 1. Get PacketType enum for T.
        // 2. Serialize T to bytes.
        // 3. Create PacketHeader.
        // 4. Create Packet, set header and payload.
        // 5. Serialize Packet to bytes.
        // 6. Prepend the total length (4 bytes, network byte order).
        // 7. Write to _sslStream.
    }

    private async Task ReadLoop(CancellationToken token) 
    {
        while (!token.IsCancellationRequested) 
        {
            // 1. Read 4 bytes for packet length.
            // 2. Read that many bytes for the full packet.
            // 3. Deserialize into Packet message.
            // 4. Based on PacketHeader.Type, deserialize payload into specific message type (e.g., LoginResponse).
            // 5. Dispatch the message to the appropriate handler (e.g., UIManager, GameManager).
        }
    }
}
```

### Step 3.3: Game Flow Logic

Create a `GameManager.cs` to manage the application state and orchestrate the connection flow.

**Conceptual Code (`GameManager.cs`):**
```csharp
public class GameManager 
{
    private NetworkManager _networkManager;
    private Dictionary<ulong, GameObject> _remotePlayers = new Dictionary<ulong, GameObject>();

    void Start() 
    {
        _networkManager = new NetworkManager();
        // Register handlers for server messages
        // e.g., _networkManager.RegisterHandler<LoginResponse>(OnLoginResponse);
        // e.g., _networkManager.RegisterHandler<MatchFoundNotification>(OnMatchFound);
        // e.g., _networkManager.RegisterHandler<RoomStateSnapshot>(OnRoomStateSnapshot);
        // e.g., _networkManager.RegisterHandler<RoomStateDelta>(OnRoomStateDelta);
    }

    public async void LoginAndQueue() 
    {
        await _networkManager.ConnectAsync("127.0.0.1", 8080);
        
        var loginReq = new LoginRequest { Username = "unity_client" };
        _networkManager.Send(loginReq);
    }

    void OnLoginResponse(LoginResponse res) 
    {
        if (res.Success) {
            Debug.Log("Login successful! Entering matchmaking...");
            var joinReq = new JoinMatchmakingRequest { GameMode = 1 };
            _networkManager.Send(joinReq);
        }
    }

    void OnMatchFound(MatchFoundNotification notification) 
    {
        Debug.Log($"Match found! Room: {notification.RoomId}");
        // In a real client, you would disconnect and reconnect to the new room's IP/port.
        // For this demo, we assume the same server connection handles the room.
    }

    void OnRoomStateSnapshot(RoomStateSnapshot snapshot) 
    {
        Debug.Log($"Received snapshot with {snapshot.Entities.Count} entities.");
        foreach (var entityData in snapshot.Entities) {
            // Create a new cube GameObject for each entity
            GameObject playerCube = GameObject.CreatePrimitive(PrimitiveType.Cube);
            playerCube.transform.position = new Vector3(entityData.Position.X, entityData.Position.Y, entityData.Position.Z);
            _remotePlayers[entityData.EntityId] = playerCube;
        }
    }

    void OnRoomStateDelta(RoomStateDelta delta) 
    {
        foreach (var entityDelta in delta.Deltas) {
            switch (entityDelta.UpdateType) {
                case EntityDelta.Types.UpdateType.Created:
                    // Create new player cube
                    break;
                case EntityDelta.Types.UpdateType.Updated:
                    // Update position of existing cube
                    if (_remotePlayers.TryGetValue(entityDelta.EntityData.EntityId, out GameObject playerCube)) {
                        // This is where you would apply interpolation (see Ext. 06)
                        playerCube.transform.position = new Vector3(/* ... */);
                    }
                    break;
                case EntityDelta.Types.UpdateType.Destroyed:
                    // Destroy the player cube
                    if (_remotePlayers.TryGetValue(entityDelta.EntityData.EntityId, out GameObject playerCube)) {
                        GameObject.Destroy(playerCube);
                        _remotePlayers.Remove(entityDelta.EntityData.EntityId);
                    }
                    break;
            }
        }
    }
}
```

## 4. Rationale for Design

*   **Decoupled Network Layer**: `NetworkManager` handles only the raw byte-level communication and message serialization/deserialization. It knows nothing about game logic.
*   **Event-Driven Game Logic**: `GameManager` subscribes to events/messages from the `NetworkManager`. This keeps the game logic clean and reactive to server-sent information.
*   **Visual Representation**: The client's primary job is to be a "dumb" renderer of the state provided by the server. It creates, updates, and destroys `GameObject`s based on the snapshot and delta messages, trusting the server as the source of truth.
*   **Interpolation is Key**: As noted in the code, directly setting the position (`playerCube.transform.position = ...`) will result in choppy movement. A real implementation *must* use the interpolation techniques described in **Extension 06** to provide a smooth visual experience.

## 5. Verification Guide

1.  **Build the Unity Client**: Create the scripts and a simple UI with a "Login and Queue" button.
2.  **Run the Server**: Start the C++ server.
3.  **Run Two Clients**: Launch two instances of the Unity client.
4.  **Initiate Matchmaking**: Click the "Login and Queue" button on both clients.
5.  **Observe Logs**: 
    *   Watch the server logs to see the two sessions connect and the `MatchmakingManager` form a match and create a room.
    *   Watch the Unity client console logs. You should see "Login successful," "Match found," and "Received snapshot."
6.  **Observe Visuals**: In the Unity Game view for both clients, you should see two cubes appear, representing each other. If you were to implement movement on the server, you would see the cubes move in sync on both clients' screens.
