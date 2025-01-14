exception PathNotFound(string);
exception DuplicateMessageId(string);
exception DefaultMessageNotMatching(string);

let extract = (~duplicatesAllowed=false, paths) => {
  let messages = ref(StringMap.empty);

  let mapper =
    Mapper.getMapper(message => {
      let {Message.id, defaultMessage, _} = message;

      switch (messages^ |> StringMap.find_opt(id)) {
      | None => messages := messages^ |> StringMap.add(id, message)

      | Some(existingMessage)
          when
            duplicatesAllowed
            && defaultMessage == existingMessage.defaultMessage =>
        messages := messages^ |> StringMap.add(id, message)

      | Some(existingMessage)
          when
            duplicatesAllowed
            && defaultMessage != existingMessage.defaultMessage =>
        raise(DefaultMessageNotMatching(id))

      | Some(_) => raise(DuplicateMessageId(id))
      };
    });

  let extractMessages = ast => mapper.structure(mapper, ast);

  let processReasonFile = path => {
    let channel = open_in_bin(path);
    let lexbuf = Lexing.from_channel(channel);
    let ast =
      Reason_toolchain.(
        RE.implementation(lexbuf) |> To_current.copy_structure
      );
    close_in(channel);

    extractMessages(ast) |> ignore;
  };

  let rec processPath = path => {
    if (!Sys.file_exists(path)) {
      raise(PathNotFound(path));
    };

    if (Sys.is_directory(path)) {
      Sys.readdir(path)
      |> Array.iter(filename => processPath(Filename.concat(path, filename)));
    } else if (Filename.extension(path) == ".re" || Filename.extension(path) == ".res") {
      processReasonFile(path);
    };
  };

  paths |> List.iter(processPath);
  messages^
  |> StringMap.bindings
  |> List.map(((_id, message)) => message)
  |> List.sort(Message.compare);
};
