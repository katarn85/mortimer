(* Frama-C journal generated at 13:03 the 25/09/2017 *)

exception Unreachable
exception Exception of string

(* Run the user commands *)
let run () =
  File.init_from_cmdline ();
  let p_interactive = Project.create "interactive" in
  Project.set_current ~on:true p_interactive;
  File.init_from_cmdline ();
  Project.set_current ~on:true (Project.from_unique_name "default");
  Project.set_current p_interactive;
  !Db.Slicing.Project.set_project None;
  Dynamic.Parameter.String.set "-wp-prover" "coqide";
  Dynamic.Parameter.String.set "-wp-prover" "why3ide";
  Dynamic.Parameter.String.set "-wp-prover" "coqide";
  Dynamic.Parameter.String.set "-wp-prover" "alt-ergo";
  Dynamic.Parameter.String.set "-wp-prover" "none";
  begin try
    (* exception Globals.No_such_entry_point("cannot find entry point `main'.\nPlease use option `-main' for specifying a valid entry point.")
         raised on:  *)
    let __ = Callgraph.Services.get () in
    raise Unreachable
  with
  | Unreachable as exn -> raise exn
  | exn (* Globals.No_such_entry_point("cannot find entry point `main'.\nPlease use option `-main' for specifying a valid entry point.") *) ->
    (* continuing: *) raise (Exception (Printexc.to_string exn))
  end

(* Main *)
let main () =
  Journal.keep_file "./.frama-c/frama_c_journal.ml";
  try run ()
  with
  | Unreachable -> Kernel.fatal "Journal reaches an assumed dead code" 
  | Exception s -> Kernel.log "Journal re-raised the exception %S" s
  | exn ->
    Kernel.fatal
      "Journal raised an unexpected exception: %s"
      (Printexc.to_string exn)

(* Registering *)
let main : unit -> unit =
  Dynamic.register
    ~plugin:"Frama_c_journal.ml"
    "main"
    (Datatype.func Datatype.unit Datatype.unit)
    ~journalize:false
    main

(* Hooking *)
let () = Cmdline.run_after_loading_stage main; Cmdline.is_going_to_load ()
